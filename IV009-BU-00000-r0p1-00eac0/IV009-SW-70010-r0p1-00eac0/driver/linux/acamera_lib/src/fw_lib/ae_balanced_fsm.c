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
#include "ae_balanced_fsm.h"

#if defined( ISP_HAS_SBUF_FSM ) || defined( ISP_HAS_USER2KERNEL_FSM )
#include "sbuf.h"
#endif

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AE_BALANCED
#endif

void AE_fsm_clear( AE_fsm_t *p_fsm )
{
    p_fsm->error_log2 = 0;
    p_fsm->ae_hist_mean = 0;
    p_fsm->exposure_log2 = 0;
    p_fsm->integrator = 0;
    p_fsm->exposure_ratio = 64;
    p_fsm->exposure_ratio_avg = 64;
    p_fsm->ae_roi_api = AE_CENTER_ZONES;
    p_fsm->roi = AE_CENTER_ZONES;
    p_fsm->save_hist_api = 0x0;
#if FW_ZONE_AE
    p_fsm->smart_zone_enable = 1;
    p_fsm->x1 = ( (AE_CENTER_ZONES)&0xFF );
    p_fsm->y1 = ( ( AE_CENTER_ZONES >> 8 ) & 0xFF );
    p_fsm->x2 = ( ( AE_CENTER_ZONES >> 16 ) & 0xFF );
    p_fsm->y2 = ( ( AE_CENTER_ZONES >> 24 ) & 0xFF );
#endif
}

void AE_request_interrupt( AE_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void AE_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    AE_fsm_t *p_fsm = (AE_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    AE_fsm_clear( p_fsm );

    ae_initialize( p_fsm );

    AE_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_AE_STATS ) );
}


int AE_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    AE_fsm_t *p_fsm = (AE_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_AE_INIT:
        AE_fsm_clear( p_fsm );
        ae_initialize( p_fsm );
        break;

#if defined( ISP_HAS_SBUF_FSM ) || defined( ISP_HAS_USER2KERNEL_FSM )
    case FSM_PARAM_SET_AE_STATS: {
        if ( !input || input_size != sizeof( sbuf_ae_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        sbuf_ae_t *p_sbuf_ae = (sbuf_ae_t *)input;

        uint32_t len = sizeof( p_sbuf_ae->stats_data );
#if DEBUG_SBUF_AE
        {
            int i;
            uint32_t histsum = 0;
            LOG( LOG_DEBUG, "len: %u, p_sbuf_ae: %p.", len, p_sbuf_ae );
            for ( i = 0; i < len / 4; i++ ) {
                histsum += p_sbuf_ae->stats_data[i];
                if ( !( i & 0x7F ) ) {
                    LOG( LOG_DEBUG, "fullhist[%d]: %u.", i, p_sbuf_ae->stats_data[i] );
                }
            }

            if ( histsum != p_sbuf_ae->histogram_sum ) {
                LOG( LOG_ERR, "Error: report histgram(%u) is not match with data calculated(%u).", p_sbuf_ae->histogram_sum, histsum );
            }
        }
#endif
        system_memcpy( p_fsm->fullhist, p_sbuf_ae->stats_data, len );
        /* The last element is the histogram sum */
        p_fsm->fullhist_sum = p_sbuf_ae->histogram_sum;
        fsm_raise_event( p_fsm, event_id_ae_stats_ready );

        break;
    }
#endif

    case FSM_PARAM_SET_AE_ROI: {
        if ( !input || input_size != sizeof( fsm_param_roi_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_roi_t *p_new = (fsm_param_roi_t *)input;
        p_fsm->ae_roi_api = p_new->roi_api;
        p_fsm->roi = p_new->roi;

        ae_roi_update( p_fsm );

        break;
    }

    case FSM_PARAM_SET_AE_ADJUST_EXP:
        if ( !input || input_size != sizeof( int32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        ae_exposure_correction( p_fsm, *(int32_t *)input );
        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


int AE_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    AE_fsm_t *p_fsm = (AE_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_AE_INFO: {
        if ( !output || output_size != sizeof( fsm_param_ae_info_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_ae_info_t *p_ae_info = (fsm_param_ae_info_t *)output;

        p_ae_info->exposure_log2 = p_fsm->exposure_log2;
        p_ae_info->ae_hist_mean = p_fsm->ae_hist_mean;
        p_ae_info->exposure_ratio = p_fsm->exposure_ratio;
        p_ae_info->error_log2 = p_fsm->error_log2;
        break;
    }

    case FSM_PARAM_GET_AE_HIST_INFO: {
        if ( !output || output_size != sizeof( fsm_param_ae_hist_info_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_ae_hist_info_t *p_hist_info = (fsm_param_ae_hist_info_t *)output;

        p_hist_info->fullhist_sum = p_fsm->fullhist_sum;
        p_hist_info->fullhist = p_fsm->fullhist;
        p_hist_info->frame_id = p_fsm->cur_using_stats_frame_id;

        break;
    }

    case FSM_PARAM_GET_AE_ROI: {
        if ( !output || output_size != sizeof( fsm_param_roi_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_roi_t *p_current = (fsm_param_roi_t *)output;
        p_current->roi_api = p_fsm->ae_roi_api;
        p_current->roi = p_fsm->roi;

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t AE_fsm_process_event( AE_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_ae_stats_ready:
        ae_calculate_target( p_fsm );
        ae_calculate_exposure( p_fsm );
        fsm_raise_event( p_fsm, event_id_exposure_changed );
        AE_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_AE_STATS ) );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
