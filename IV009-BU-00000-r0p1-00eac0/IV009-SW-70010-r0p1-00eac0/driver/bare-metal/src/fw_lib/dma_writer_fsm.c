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
#include "dma_writer_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_DMA_WRITER
#endif

void dma_writer_fsm_clear( dma_writer_fsm_t *p_fsm )
{
    p_fsm->dma_reader_out = dma_fr;
    p_fsm->context_created = 0;
    p_fsm->dis_ds_base_offset = 0;
    p_fsm->handle = NULL;
}

void dma_writer_request_interrupt( dma_writer_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void dma_writer_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    dma_writer_fsm_t *p_fsm = (dma_writer_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    dma_writer_fsm_clear( p_fsm );

    p_fsm->state = dma_writer_state_wait_for_sensor;
    frame_buffer_initialize( p_fsm );
}

int dma_writer_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    dma_writer_fsm_t *p_fsm = (dma_writer_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_DMA_PIPE_SETTING: {
        fsm_param_dma_pipe_setting_t *p_pipe_setting = NULL;
        if ( !input || input_size != sizeof( fsm_param_dma_pipe_setting_t ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        /*
         * For now p_pipe_setting is unused. We may want to use it in future
         */
        (void)p_pipe_setting;

        fsm_raise_event( p_fsm, event_id_frame_buf_reinit );

        break;
    }

    case FSM_PARAM_SET_DMA_READER_OUTPUT:
        if ( !input || input_size != sizeof( dma_type ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->dma_reader_out = *(dma_type *)input;
        acamera_frame_buffer_update( p_fsm );

        break;

    case FSM_PARAM_SET_DMA_VFLIP:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->vflip = *(uint32_t *)input;
        acamera_frame_buffer_update( p_fsm );

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}

int dma_writer_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    dma_writer_fsm_t *p_fsm = (dma_writer_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_DMA_READER_OUTPUT:
        if ( !output || output_size != sizeof( dma_type ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(dma_type *)output = p_fsm->dma_reader_out;

        break;

    case FSM_PARAM_GET_DMA_VFLIP:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->vflip;

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t dma_writer_fsm_process_event( dma_writer_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_frame_buffer_fr_ready:
        if ( p_fsm->state == dma_writer_state_frame_processing_wait_events ) {
            p_fsm->state = dma_writer_state_frame_processing_FR_finished;
            frame_buffer_fr_finished( p_fsm );
            p_fsm->state = dma_writer_state_frame_processing_wait_events;
            b_event_processed = 1;
        }
        break;
    case event_id_frame_buffer_ds_ready:
        if ( p_fsm->state == dma_writer_state_frame_processing_wait_events ) {
            p_fsm->state = dma_writer_state_frame_processing_DS_finished;
            frame_buffer_ds_finished( p_fsm );
            p_fsm->state = dma_writer_state_frame_processing_wait_events;
            b_event_processed = 1;
        }
        break;
#ifdef ISP_HAS_CROP_FSM
    case event_id_crop_updated:
#else
    case event_id_sensor_ready:
#endif
        if ( ( p_fsm->state == dma_writer_state_wait_for_sensor ) ||
             ( p_fsm->state == dma_writer_state_frame_processing_wait_events ) ) {
            p_fsm->state = dma_writer_state_frame_processing_init;
            acamera_frame_buffer_update( p_fsm );
            p_fsm->state = dma_writer_state_frame_processing_wait_events;
            b_event_processed = 1;
        }
        break;
    case event_id_frame_buf_reinit:
        if ( p_fsm->state == dma_writer_state_frame_processing_wait_events ) {
            p_fsm->state = dma_writer_state_frame_processing_buf_reinit;
            acamera_frame_buffer_update( p_fsm );
            p_fsm->state = dma_writer_state_frame_processing_wait_events;
            b_event_processed = 1;
        }
        break;
    case event_id_sensor_not_ready:
        if ( ( p_fsm->state >= dma_writer_state_frame_processing ) && ( p_fsm->state < next_to_dma_writer_state_frame_processing ) ) {
            p_fsm->state = dma_writer_state_wait_for_sensor;
            b_event_processed = 1;
        }
        break;
    case event_id_frame_buffer_metadata:
        if ( p_fsm->state == dma_writer_state_frame_processing_wait_events ) {
            p_fsm->state = dma_writer_state_frame_processing_prepare_metadata;
            frame_buffer_prepare_metadata( p_fsm );
            p_fsm->state = dma_writer_state_frame_processing_wait_events;
            b_event_processed = 1;
        }
        break;
    }

    return b_event_processed;
}
