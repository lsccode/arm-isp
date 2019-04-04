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
#include "iridix8_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_IRIDIX8
#endif

void iridix_fsm_clear( iridix_fsm_t *p_fsm )
{
    p_fsm->strength_target = IRIDIX_STRENGTH_TARGET;
    p_fsm->strength_avg = IRIDIX_STRENGTH_TARGET * CALIBRATION_IRIDIX_AVG_COEF_INIT;
    p_fsm->mode = 0;
    p_fsm->dark_enh = 15000;
    p_fsm->dark_enh_avg = IRIDIX_STRENGTH_TARGET * CALIBRATION_IRIDIX_AVG_COEF_INIT * 2;
    p_fsm->iridix_global_DG_avg = IRIDIX_STRENGTH_TARGET * CALIBRATION_IRIDIX_AVG_COEF_INIT * 2;
    p_fsm->mp_iridix_strength = 0;
    p_fsm->iridix_contrast = 256;
    p_fsm->iridix_global_DG = 256;
    p_fsm->iridix_global_DG_prev = 256;

    p_fsm->frame_id_tracking = 0;
}

void iridix_request_interrupt( iridix_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void iridix_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    iridix_fsm_t *p_fsm = (iridix_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    iridix_fsm_clear( p_fsm );

    iridix_initialize( p_fsm );
}


int iridix_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    iridix_fsm_t *p_fsm = (iridix_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_IRIDIX_INIT: {
        iridix_initialize( p_fsm );
        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


int iridix_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    iridix_fsm_t *p_fsm = (iridix_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_IRIDIX_STRENGTH: {
        if ( !output || output_size != sizeof( uint16_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint16_t *)output = p_fsm->strength_target;

        break;
    }

    case FSM_PARAM_GET_IRIDIX_CONTRAST: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->iridix_contrast;

        break;
    }

    case FSM_PARAM_GET_IRIDIX_DARK_ENH: {
        if ( !output || output_size != sizeof( uint16_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint16_t *)output = p_fsm->dark_enh;

        break;
    }

    case FSM_PARAM_GET_IRIDIX_GLOBAL_DG: {
        if ( !output || output_size != sizeof( uint16_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint16_t *)output = p_fsm->iridix_global_DG;

        break;
    }
    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t iridix_fsm_process_event( iridix_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_update_iridix:
        iridix_update( p_fsm );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
