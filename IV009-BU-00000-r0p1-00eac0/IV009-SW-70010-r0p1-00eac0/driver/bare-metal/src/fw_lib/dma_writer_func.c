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
#include "acamera_math.h"
#include "acamera_firmware_config.h"
#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_config.h"
#endif
#include "acamera_isp_config.h"
#include "acamera_command_api.h"
#include "dma_writer_api.h"
#include "system_stdlib.h"
#include "dma_writer_fsm.h"


#define NUMBER_OF_USED_BANKS 3

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_DMA_WRITER
#endif


void dma_writer_fsm_process_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{

    LOG( LOG_DEBUG, "dma writer fsm irq event %d", (int)irq_event );
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    // send this interrupt to each pipe
    dma_writer_process_interrupt( p_fsm->handle, irq_event );
    switch ( irq_event ) {
    case ACAMERA_IRQ_FRAME_WRITER_FR:

        fsm_raise_event( p_fsm, event_id_frame_buffer_fr_ready );
        fsm_raise_event( p_fsm, event_id_frame_buffer_metadata );

        break;
    case ACAMERA_IRQ_FRAME_WRITER_DS:
        fsm_raise_event( p_fsm, event_id_frame_buffer_ds_ready );

        break;
    }

    return;
}

void acamera_frame_buffer_update( dma_writer_fsm_const_ptr_t p_fsm )
{
    dma_pipe_settings set_pipe;
    dma_error result = edma_ok;
    fsm_param_crop_info_t crop_info;
#ifdef ISP_HAS_CROP_FSM
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CROP_INFO, NULL, 0, &crop_info, sizeof( crop_info ) );
#else
    crop_info.width_fr = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
    crop_info.height_fr = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
#if ISP_HAS_DS1
    crop_info.width_ds = crop_info.width_fr;
    crop_info.height_ds = crop_info.height_fr;
#endif
#endif
    system_memset( &set_pipe, 0, sizeof( set_pipe ) );
    dma_writer_get_settings( p_fsm->handle, dma_fr, &set_pipe );

    set_pipe.height = crop_info.height_fr;
    set_pipe.width = crop_info.width_fr;

    set_pipe.active = ( p_fsm->dma_reader_out == dma_fr );
    set_pipe.vflip = p_fsm->vflip;
    LOG( LOG_INFO, "fr update crop %d x %d", (int)crop_info.width_fr, (int)crop_info.height_fr );
    result |= dma_writer_set_settings( p_fsm->handle, dma_fr, &set_pipe );

    if ( result == edma_ok )
        dma_writer_set_initialized( p_fsm->handle, dma_fr, 1 );
    else
        dma_writer_set_initialized( p_fsm->handle, dma_fr, 0 );
#if ISP_HAS_DS1
    result = edma_ok;
    system_memset( &set_pipe, 0, sizeof( set_pipe ) );
    dma_writer_get_settings( p_fsm->handle, dma_ds1, &set_pipe );
    set_pipe.height = crop_info.height_ds;
    set_pipe.width = crop_info.width_ds;

    set_pipe.active = ( p_fsm->dma_reader_out == dma_ds1 );
    set_pipe.vflip = p_fsm->vflip;
    LOG( LOG_INFO, "ds updated crop %d x %d", (int)set_pipe.width, (int)set_pipe.height );
    result |= dma_writer_set_settings( p_fsm->handle, dma_ds1, &set_pipe );

    if ( result == edma_ok )
        dma_writer_set_initialized( p_fsm->handle, dma_ds1, 1 );
    else
        dma_writer_set_initialized( p_fsm->handle, dma_ds1, 0 );
#endif
}

void frame_buffer_initialize( dma_writer_fsm_ptr_t p_fsm )
{
    dma_api api_ops;
    dma_pipe_settings set_pipe;
    // register interrupts
    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_START ) | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_WRITER_FR ) | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_WRITER_DS );

    dma_writer_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );

// DS buffer
#if ISP_HAS_FPGA_WRAPPER && ISP_CONTROLS_DMA_READER
    //  FRAME READER tab (address will be initialized in the first interrupt
    acamera_fpga_frame_reader_axi_port_enable_write( p_fsm->cmn.isp_base, 1 );
#endif

    if ( p_fsm->context_created == 0 ) {
        // create dma_pipe_hanlde
        dma_writer_create( &p_fsm->handle );
        p_fsm->context_created = 1;
        p_fsm->dma_reader_out = dma_fr; //default out to dma reader
    }
    if ( p_fsm->handle != NULL ) {
        dma_error result = edma_ok;
        fsm_param_crop_info_t crop_info;
#ifdef ISP_HAS_CROP_FSM
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CROP_INFO, NULL, 0, &crop_info, sizeof( crop_info ) );
        LOG( LOG_DEBUG, "crop settings %d x %d", (int)crop_info.width_fr, (int)crop_info.height_fr );
#else
        crop_info.width_fr = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
        crop_info.height_fr = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
#if ISP_HAS_DS1
        crop_info.width_ds = crop_info.width_fr;
        crop_info.height_ds = crop_info.height_fr;
        LOG( LOG_DEBUG, "crop settings %d x %d and %d x %d", (int)crop_info.width_fr, (int)crop_info.height_fr, (int)crop_info.width_ds, (int)crop_info.height_ds );
#endif

#endif

        // initialize fr pipe
        system_memset( &set_pipe, 0, sizeof( set_pipe ) );
        system_memset( &api_ops, 0, sizeof( api_ops ) );
        // api
        api_ops.p_acamera_isp_dma_writer_format_read = acamera_isp_fr_dma_writer_format_read;
        api_ops.p_acamera_isp_dma_writer_format_write = acamera_isp_fr_dma_writer_format_write;
        api_ops.p_acamera_isp_dma_writer_max_bank_write = acamera_isp_fr_dma_writer_max_bank_write;
        api_ops.p_acamera_isp_dma_writer_bank0_base_write = acamera_isp_fr_dma_writer_bank0_base_write;
#if ISP_HAS_FPGA_WRAPPER
        api_ops.p_acamera_fpga_frame_reader_rbase_write = acamera_fpga_frame_reader_rbase_write;
        api_ops.p_acamera_fpga_frame_reader_line_offset_write = acamera_fpga_frame_reader_line_offset_write;
        api_ops.p_acamera_fpga_frame_reader_format_write = acamera_fpga_frame_reader_format_write;
        api_ops.p_acamera_fpga_frame_reader_rbase_load_write = acamera_fpga_frame_reader_rbase_load_write;
#endif
        api_ops.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_fr_dma_writer_line_offset_write;
        api_ops.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_fr_dma_writer_line_offset_write;

        api_ops.p_acamera_isp_dma_writer_frame_write_on_write = acamera_isp_fr_dma_writer_frame_write_on_write;
        api_ops.p_acamera_isp_dma_writer_active_width_write = acamera_isp_fr_dma_writer_active_width_write;
        api_ops.p_acamera_isp_dma_writer_active_height_write = acamera_isp_fr_dma_writer_active_height_write;
        api_ops.p_acamera_isp_dma_writer_active_width_read = acamera_isp_fr_dma_writer_active_width_read;
        api_ops.p_acamera_isp_dma_writer_active_height_read = acamera_isp_fr_dma_writer_active_height_read;


        api_ops.p_acamera_isp_dma_writer_format_read_uv = acamera_isp_fr_uv_dma_writer_format_read;
        api_ops.p_acamera_isp_dma_writer_format_write_uv = acamera_isp_fr_uv_dma_writer_format_write;
        api_ops.p_acamera_isp_dma_writer_bank0_base_write_uv = acamera_isp_fr_uv_dma_writer_bank0_base_write;
        api_ops.p_acamera_isp_dma_writer_max_bank_write_uv = acamera_isp_fr_uv_dma_writer_max_bank_write;
        api_ops.p_acamera_isp_dma_writer_line_offset_write_uv = acamera_isp_fr_uv_dma_writer_line_offset_write;
        api_ops.p_acamera_isp_dma_writer_frame_write_on_write_uv = acamera_isp_fr_uv_dma_writer_frame_write_on_write;
        api_ops.p_acamera_isp_dma_writer_active_width_write_uv = acamera_isp_fr_uv_dma_writer_active_width_write;
        api_ops.p_acamera_isp_dma_writer_active_height_write_uv = acamera_isp_fr_uv_dma_writer_active_height_write;
        api_ops.p_acamera_isp_dma_writer_active_width_read_uv = acamera_isp_fr_uv_dma_writer_active_width_read;
        api_ops.p_acamera_isp_dma_writer_active_height_read_uv = acamera_isp_fr_uv_dma_writer_active_height_read;
#if ISP_HAS_FPGA_WRAPPER && defined( ACAMERA_FPGA_FRAME_READER_UV_RBASE_DEFAULT )
        api_ops.p_acamera_fpga_frame_reader_rbase_write_uv = acamera_fpga_frame_reader_uv_rbase_write;
        api_ops.p_acamera_fpga_frame_reader_line_offset_write_uv = acamera_fpga_frame_reader_uv_line_offset_write;
        api_ops.p_acamera_fpga_frame_reader_format_write_uv = acamera_fpga_frame_reader_uv_format_write;
        api_ops.p_acamera_fpga_frame_reader_rbase_load_write_uv = acamera_fpga_frame_reader_uv_rbase_load_write;
#endif
        // settings
        dma_writer_get_settings( p_fsm->handle, dma_fr, &set_pipe );
        set_pipe.p_ctx = ACAMERA_FSM2CTX_PTR( p_fsm ); //back reference

        set_pipe.height = crop_info.height_fr;
        set_pipe.width = crop_info.width_fr;

        set_pipe.isp_base = p_fsm->cmn.isp_base;
        set_pipe.ctx_id = ACAMERA_FSM2CTX_PTR( p_fsm )->context_id;

        set_pipe.active = ( p_fsm->dma_reader_out == dma_fr );
        set_pipe.vflip = p_fsm->vflip;
        LOG( LOG_INFO, "fr init crop %d x %d", (int)crop_info.width_fr, (int)crop_info.height_fr );
        result |= dma_writer_init( p_fsm->handle, dma_fr, &set_pipe, &api_ops );


        if ( result == edma_ok )
            dma_writer_set_initialized( p_fsm->handle, dma_fr, 1 );
        else
            dma_writer_set_initialized( p_fsm->handle, dma_fr, 0 );


#if ISP_HAS_DS1
        // initialize ds pipe
        result = edma_ok;
        system_memset( &set_pipe, 0, sizeof( set_pipe ) );
        system_memset( &api_ops, 0, sizeof( api_ops ) );
        // api
        api_ops.p_acamera_isp_dma_writer_format_read = acamera_isp_ds1_dma_writer_format_read;
        api_ops.p_acamera_isp_dma_writer_format_write = acamera_isp_ds1_dma_writer_format_write;
        api_ops.p_acamera_isp_dma_writer_max_bank_write = acamera_isp_ds1_dma_writer_max_bank_write;
        api_ops.p_acamera_isp_dma_writer_bank0_base_write = acamera_isp_ds1_dma_writer_bank0_base_write;
#if ISP_HAS_FPGA_WRAPPER
        api_ops.p_acamera_fpga_frame_reader_rbase_write = acamera_fpga_frame_reader_rbase_write;
        api_ops.p_acamera_fpga_frame_reader_line_offset_write = acamera_fpga_frame_reader_line_offset_write;
        api_ops.p_acamera_fpga_frame_reader_format_write = acamera_fpga_frame_reader_format_write;
        api_ops.p_acamera_fpga_frame_reader_rbase_load_write = acamera_fpga_frame_reader_rbase_load_write;
#endif
        api_ops.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_ds1_dma_writer_line_offset_write;
        api_ops.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_ds1_dma_writer_line_offset_write;

        api_ops.p_acamera_isp_dma_writer_frame_write_on_write = acamera_isp_ds1_dma_writer_frame_write_on_write;
        api_ops.p_acamera_isp_dma_writer_active_width_write = acamera_isp_ds1_dma_writer_active_width_write;
        api_ops.p_acamera_isp_dma_writer_active_height_write = acamera_isp_ds1_dma_writer_active_height_write;
        api_ops.p_acamera_isp_dma_writer_active_width_read = acamera_isp_ds1_dma_writer_active_width_read;
        api_ops.p_acamera_isp_dma_writer_active_height_read = acamera_isp_ds1_dma_writer_active_height_read;


        api_ops.p_acamera_isp_dma_writer_format_read_uv = acamera_isp_ds1_uv_dma_writer_format_read;
        api_ops.p_acamera_isp_dma_writer_format_write_uv = acamera_isp_ds1_uv_dma_writer_format_write;
        api_ops.p_acamera_isp_dma_writer_bank0_base_write_uv = acamera_isp_ds1_uv_dma_writer_bank0_base_write;
        api_ops.p_acamera_isp_dma_writer_max_bank_write_uv = acamera_isp_ds1_uv_dma_writer_max_bank_write;
        api_ops.p_acamera_isp_dma_writer_line_offset_write_uv = acamera_isp_ds1_uv_dma_writer_line_offset_write;
        api_ops.p_acamera_isp_dma_writer_frame_write_on_write_uv = acamera_isp_ds1_uv_dma_writer_frame_write_on_write;
        api_ops.p_acamera_isp_dma_writer_active_width_write_uv = acamera_isp_ds1_uv_dma_writer_active_width_write;
        api_ops.p_acamera_isp_dma_writer_active_height_write_uv = acamera_isp_ds1_uv_dma_writer_active_height_write;
        api_ops.p_acamera_isp_dma_writer_active_width_read_uv = acamera_isp_ds1_uv_dma_writer_active_width_read;
        api_ops.p_acamera_isp_dma_writer_active_height_read_uv = acamera_isp_ds1_uv_dma_writer_active_height_read;
#if ISP_HAS_FPGA_WRAPPER && defined( ACAMERA_FPGA_FRAME_READER_UV_RBASE_DEFAULT )
        api_ops.p_acamera_fpga_frame_reader_rbase_write_uv = acamera_fpga_frame_reader_uv_rbase_write;
        api_ops.p_acamera_fpga_frame_reader_line_offset_write_uv = acamera_fpga_frame_reader_uv_line_offset_write;
        api_ops.p_acamera_fpga_frame_reader_format_write_uv = acamera_fpga_frame_reader_uv_format_write;
        api_ops.p_acamera_fpga_frame_reader_rbase_load_write_uv = acamera_fpga_frame_reader_uv_rbase_load_write;
#endif
        // settings
        dma_writer_get_settings( p_fsm->handle, dma_ds1, &set_pipe );
        set_pipe.p_ctx = ACAMERA_FSM2CTX_PTR( p_fsm ); //back reference
        set_pipe.height = crop_info.height_ds;
        set_pipe.width = crop_info.width_ds;

        set_pipe.isp_base = p_fsm->cmn.isp_base;
        set_pipe.ctx_id = ACAMERA_FSM2CTX_PTR( p_fsm )->context_id;
        LOG( LOG_INFO, "ds init crop %d x %d ", (int)set_pipe.width, (int)set_pipe.height );

        set_pipe.active = ( p_fsm->dma_reader_out == dma_ds1 );
        set_pipe.vflip = p_fsm->vflip;

        result |= dma_writer_init( p_fsm->handle, dma_ds1, &set_pipe, &api_ops );
        if ( result == edma_ok )
            dma_writer_set_initialized( p_fsm->handle, dma_ds1, 1 );
        else
            dma_writer_set_initialized( p_fsm->handle, dma_ds1, 0 );
#endif
    }
}

void frame_buffer_prepare_metadata( dma_writer_fsm_ptr_t p_fsm )
{
    dma_pipe_settings *p_set = NULL;

#if ISP_HAS_CMOS_FSM
    int32_t frame = NUMBER_OF_USED_BANKS;
    exposure_set_t exp_set;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame, sizeof( frame ), &exp_set, sizeof( exp_set ) );

#endif


    dma_writer_get_ptr_settings( p_fsm->handle, dma_fr, &p_set );

    if ( p_set ) {
        p_set->curr_metadata.format = acamera_isp_fr_dma_writer_format_read( p_fsm->cmn.isp_base );
        p_set->curr_metadata.width = p_set->width;
        p_set->curr_metadata.height = p_set->height;
        p_set->curr_metadata.line_size = acamera_line_offset( p_set->curr_metadata.width, _get_pixel_width( p_set->curr_metadata.format ) );
#if ISP_HAS_CMOS_FSM
        p_set->curr_metadata.exposure = exp_set.info.exposure_log2;
        p_set->curr_metadata.int_time = exp_set.data.integration_time;
        p_set->curr_metadata.int_time_medium = exp_set.data.integration_time_medium;
        p_set->curr_metadata.int_time_long = exp_set.data.integration_time_long;
        p_set->curr_metadata.again = exp_set.info.again_log2;
        p_set->curr_metadata.dgain = exp_set.info.dgain_log2;
        p_set->curr_metadata.isp_dgain = exp_set.info.isp_dgain_log2;
#endif
    }


#if ISP_HAS_DS1
    dma_writer_get_ptr_settings( p_fsm->handle, dma_ds1, &p_set );

    if ( p_set ) {
        p_set->curr_metadata.format = acamera_isp_ds1_dma_writer_format_read( p_fsm->cmn.isp_base );
        p_set->curr_metadata.width = p_set->width;
        p_set->curr_metadata.height = p_set->height;
        p_set->curr_metadata.line_size = acamera_line_offset( p_set->curr_metadata.width, _get_pixel_width( p_set->curr_metadata.format ) );
#if ISP_HAS_CMOS_FSM

        p_set->curr_metadata.exposure = exp_set.info.exposure_log2;
        p_set->curr_metadata.int_time = exp_set.data.integration_time;
        p_set->curr_metadata.int_time_medium = exp_set.data.integration_time_medium;
        p_set->curr_metadata.int_time_long = exp_set.data.integration_time_long;
        p_set->curr_metadata.again = exp_set.info.again_log2;
        p_set->curr_metadata.dgain = exp_set.info.dgain_log2;
        p_set->curr_metadata.isp_dgain = exp_set.info.isp_dgain_log2;
#endif
    }
#endif
}

void frame_buffer_fr_finished( dma_writer_fsm_ptr_t p_fsm )
{
    metadata_t *metadata_cb = dma_writer_return_metadata( p_fsm->handle, dma_fr );

    if ( metadata_cb ) {
        metadata_cb->frame_id = ACAMERA_FSM2CTX_PTR( p_fsm )->isp_frame_counter;
    }
}

void frame_buffer_ds_finished( dma_writer_fsm_ptr_t p_fsm )
{
#if ISP_HAS_DS1
    metadata_t *metadata_cb = dma_writer_return_metadata( p_fsm->handle, dma_ds1 );
    // scale DS DIS

    if ( !acamera_isp_top_bypass_ds1_scaler_read( p_fsm->cmn.isp_base ) ) {
        if ( ( acamera_isp_ds1_scaler_hfilt_tinc_read( p_fsm->cmn.isp_base ) != 0 ) && ( acamera_isp_ds1_scaler_vfilt_tinc_read( p_fsm->cmn.isp_base ) != 0 ) ) {
            metadata_t *metadata_fr = dma_writer_return_metadata( p_fsm->handle, dma_ds1 );
            if ( metadata_fr ) {
                metadata_cb->dis_offset_x = ( int8_t )( ( (int32_t)metadata_fr->dis_offset_x << 20 ) / (int32_t)acamera_isp_ds1_scaler_hfilt_tinc_read( p_fsm->cmn.isp_base ) ); // division by zero is checked
                metadata_cb->dis_offset_y = ( int8_t )( ( (int32_t)metadata_fr->dis_offset_y << 20 ) / (int32_t)acamera_isp_ds1_scaler_vfilt_tinc_read( p_fsm->cmn.isp_base ) ); // division by zero is checked
            }
        } else {
            LOG( LOG_ERR, "AVOIDED DIVISION BY ZERO: acamera_isp_ds1_scaler_hfilt_tinc_read: %lu, acamera_isp_ds1_scaler_vfilt_tinc_read: %lu", acamera_isp_ds1_scaler_hfilt_tinc_read( p_fsm->cmn.isp_base ), acamera_isp_ds1_scaler_vfilt_tinc_read( p_fsm->cmn.isp_base ) );
        }
    }

    if ( metadata_cb ) {
        metadata_cb->frame_id = ACAMERA_FSM2CTX_PTR( p_fsm )->isp_frame_counter;
    }

#endif
}

void dma_writer_update_address_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;
    // send this interrupt to each pipe
    dma_writer_process_interrupt( p_fsm->handle, irq_event );
}

void dma_writer_deinit( dma_writer_fsm_ptr_t p_fsm )
{
    dma_writer_exit( p_fsm->handle );
}
