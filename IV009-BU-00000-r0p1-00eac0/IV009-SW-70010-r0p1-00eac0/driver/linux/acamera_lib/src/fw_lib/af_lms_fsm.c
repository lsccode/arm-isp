//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT [2018] ARM Limited or its affiliates.
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#include "acamera_fw.h"
#include "af_lms_fsm.h"

#if defined( ISP_HAS_SBUF_FSM ) || defined( ISP_HAS_USER2KERNEL_FSM )
#include "sbuf.h"
#endif

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AF_LMS
#endif

void AF_fsm_clear( AF_fsm_t *p_fsm )
{
    p_fsm->frame_num = 10;
    p_fsm->mode = AF_MODE_AF;
    p_fsm->pos_manual = 0;
    p_fsm->roi = 0x4040C0C0;
    p_fsm->lens_driver_ok = 0;
    p_fsm->roi_api = 0x4040C0C0;
    p_fsm->af_status = AF_STATUS_INVALID;
    p_fsm->last_sharp = 0;
    memset( &p_fsm->lens_ctrl, 0, sizeof( p_fsm->lens_ctrl ) );
    memset( &p_fsm->lens_param, 0, sizeof( p_fsm->lens_param ) );
}

void AF_request_interrupt( AF_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void AF_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    AF_fsm_t *p_fsm = (AF_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    AF_fsm_clear( p_fsm );

    AF_init( p_fsm );
}


int AF_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    AF_fsm_t *p_fsm = (AF_fsm_t *)fsm;

    if ( !p_fsm->lens_driver_ok ) {
        LOG( LOG_INFO, "Not supported: no lens driver." );
        return -1;
    }

    switch ( param_id ) {
#if defined( ISP_HAS_SBUF_FSM ) || defined( ISP_HAS_USER2KERNEL_FSM )
    case FSM_PARAM_SET_AF_STATS: {
        if ( !input || input_size != sizeof( sbuf_af_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        sbuf_af_t *p_sbuf_af = (sbuf_af_t *)input;

        p_fsm->skip_cur_frame = p_sbuf_af->skip_cur_frame;
        p_fsm->frame_num = p_sbuf_af->frame_num;
        system_memcpy( p_fsm->zone_raw_statistic, p_sbuf_af->stats_data, sizeof( p_fsm->zone_raw_statistic ) );
        fsm_raise_event( p_fsm, event_id_af_stats_ready );

        break;
    }
#endif

    case FSM_PARAM_SET_AF_MODE:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->mode = *(uint32_t *)input;
        fsm_raise_event( p_fsm, event_id_af_refocus );

        break;

    case FSM_PARAM_SET_AF_MANUAL_POS: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t pos_manual = *(uint32_t *)input;

        if ( pos_manual <= 256 ) {
            p_fsm->pos_manual = pos_manual;
            fsm_raise_event( p_fsm, event_id_af_refocus );
        }

        break;
    }

    case FSM_PARAM_SET_AF_RANGE_LOW: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t value = *(uint32_t *)input;

        if ( value <= 256 ) {
            af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
            p_fsm->pos_min = param->pos_min + ( uint16_t )( ( uint32_t )( param->pos_max - param->pos_min ) * value >> 8 );
            fsm_raise_event( p_fsm, event_id_af_refocus );
        }
        break;
    }

    case FSM_PARAM_SET_AF_RANGE_HIGH: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t value = *(uint32_t *)input;

        if ( value <= 256 ) {
            af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
            p_fsm->pos_max = param->pos_min + ( uint16_t )( ( uint32_t )( param->pos_max - param->pos_min ) * value >> 8 );
            fsm_raise_event( p_fsm, event_id_af_refocus );
        }

        break;
    }

    case FSM_PARAM_SET_AF_ROI: {
        if ( !input || input_size != sizeof( fsm_param_roi_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_roi_t *p_new = (fsm_param_roi_t *)input;
        p_fsm->roi_api = p_new->roi_api;
        p_fsm->roi = p_new->roi;

        fsm_raise_event( p_fsm, event_id_af_refocus );

        break;
    }

    case FSM_PARAM_SET_AF_LENS_REG: {
        if ( !input || input_size != sizeof( fsm_param_reg_cfg_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_reg_cfg_t *p_cfg = (fsm_param_reg_cfg_t *)input;
        p_fsm->lens_ctrl.write_lens_register( p_fsm->lens_ctx, p_cfg->reg_addr, p_cfg->reg_value );
        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


int AF_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    AF_fsm_t *p_fsm = (AF_fsm_t *)fsm;

    if ( !p_fsm->lens_driver_ok ) {
        LOG( LOG_INFO, "Not supported: no lens driver." );
        return -1;
    }

    switch ( param_id ) {
    case FSM_PARAM_GET_AF_INFO: {
        if ( !output || output_size != sizeof( fsm_param_af_info_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_af_info_t *p_af_info = (fsm_param_af_info_t *)output;

        p_af_info->last_position = p_fsm->last_position;
        p_af_info->last_sharp = p_fsm->last_sharp;
        p_af_info->skip_frame = p_fsm->skip_frame;
        break;
    }

    case FSM_PARAM_GET_AF_MODE:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->mode;

        break;

    case FSM_PARAM_GET_LENS_PARAM: {
        if ( !output || output_size != sizeof( lens_param_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        const lens_param_t *p_lens_param = p_fsm->lens_ctrl.get_parameters( p_fsm->lens_ctx );

        *(lens_param_t *)output = *p_lens_param;

        break;
    }

    case FSM_PARAM_GET_AF_MANUAL_POS: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint16_t af_range = p_fsm->pos_max - p_fsm->pos_min;
        uint32_t last_position = p_fsm->last_position;
        uint32_t pos = 0;

        if ( last_position < p_fsm->pos_min ) {
            last_position = p_fsm->pos_min;
        } else if ( last_position > p_fsm->pos_max ) {
            last_position = p_fsm->pos_max;
        }

        pos = ( ( ( last_position - p_fsm->pos_min ) << 8 ) + af_range / 2 ) / ( af_range );

        if ( pos > 255 ) {
            pos = 255;
        }

        *(uint32_t *)output = pos;

        break;
    }

    case FSM_PARAM_GET_AF_RANGE_LOW: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
        *(uint32_t *)output = ( param->pos_max != param->pos_min ) ? ( ( uint32_t )( p_fsm->pos_min - param->pos_min ) << 8 ) / ( param->pos_max - param->pos_min ) : -1;

        break;
    }

    case FSM_PARAM_GET_AF_RANGE_HIGH: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
        *(uint32_t *)output = ( param->pos_max != param->pos_min ) ? ( ( uint32_t )( p_fsm->pos_max - param->pos_min ) << 8 ) / ( param->pos_max - param->pos_min ) : -1;

        break;
    }

    case FSM_PARAM_GET_AF_ROI: {
        if ( !output || output_size != sizeof( fsm_param_roi_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_roi_t *p_current = (fsm_param_roi_t *)output;
        p_current->roi_api = p_fsm->roi_api;
        p_current->roi = p_fsm->roi;
        break;
    }

    case FSM_PARAM_GET_AF_LENS_REG:
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->lens_ctrl.read_lens_register( p_fsm->lens_ctx, *(uint32_t *)input );
        break;

    case FSM_PARAM_GET_AF_LENS_STATUS:
        if ( !output || output_size != sizeof( int32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(int32_t *)output = p_fsm->lens_driver_ok;
        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t AF_fsm_process_event( AF_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;

    if ( !p_fsm->lens_driver_ok ) {
        LOG( LOG_INFO, "Not supported: no lens driver." );
        return 0;
    }

    switch ( event_id ) {
    default:
        break;

    case event_id_af_stats_ready:
        if ( AF_STATUS_FAST_SEARCH == p_fsm->af_status ) {
            AF_process_fast_search_step( p_fsm );
            b_event_processed = 1;
        } else if ( AF_STATUS_TRACKING == p_fsm->af_status ) {
            AF_process_track_position_step( p_fsm );
            b_event_processed = 1;
        }
        break;

    case event_id_af_step:
        if ( ( AF_STATUS_FAST_SEARCH == p_fsm->af_status ) ||
             ( AF_STATUS_TRACKING == p_fsm->af_status ) ) {
            AF_request_irq( p_fsm );
            b_event_processed = 1;
        }
        break;

    case event_id_af_fast_search_finished:
        if ( AF_STATUS_FAST_SEARCH == p_fsm->af_status ) {
            AF_init_track_position( p_fsm );
            p_fsm->af_status = AF_STATUS_TRACKING;

            AF_request_irq( p_fsm );
            b_event_processed = 1;
        }
        break;

    case event_id_af_converged:
        if ( ( AF_STATUS_FAST_SEARCH == p_fsm->af_status ) ||
             ( AF_STATUS_TRACKING == p_fsm->af_status ) ) {
            p_fsm->af_status = AF_STATUS_CONVERGED;
            b_event_processed = 1;
        }
        break;

    case event_id_af_refocus:
        if ( AF_STATUS_FAST_SEARCH == p_fsm->af_status ) {
            b_event_processed = 1;
            break;
        }

        if ( ( AF_STATUS_TRACKING == p_fsm->af_status ) ||
             ( AF_STATUS_CONVERGED == p_fsm->af_status ) ) {
            AF_init_fast_search( p_fsm );
            p_fsm->af_status = AF_STATUS_FAST_SEARCH;
            AF_request_irq( p_fsm );

            b_event_processed = 1;
        }
        break;

    case event_id_sensor_ready:
        if ( AF_STATUS_INVALID == p_fsm->af_status ) {
            AF_init_fast_search( p_fsm );
            p_fsm->af_status = AF_STATUS_FAST_SEARCH;
            AF_request_irq( p_fsm );

            b_event_processed = 1;
        }
        break;

    case event_id_sensor_not_ready:
        if ( p_fsm->af_status != AF_STATUS_INVALID ) {
            p_fsm->af_status = AF_STATUS_INVALID;
            AF_init( p_fsm );
            b_event_processed = 1;
        }
        break;
    }

    return b_event_processed;
}
