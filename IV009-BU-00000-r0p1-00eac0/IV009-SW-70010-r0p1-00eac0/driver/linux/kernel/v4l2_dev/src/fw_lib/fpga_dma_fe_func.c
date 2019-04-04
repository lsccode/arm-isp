/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "acamera_firmware_config.h"
#include "acamera_fw.h"
#include "acamera_types.h"
#include "acamera_fw.h"
#include "dfe_control_api.h"
#include "fpga_dma_fe_fsm.h"

#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_fe_isp1_config.h"
#include "acamera_fpga_fe_isp2_config.h"
#include "acamera_fpga_fe_isp3_config.h"
#include "acamera_fpga_fe_isp4_config.h"
#endif

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_FPGA_DMA_FE
#endif

void fpga_dma_fe_stop( fpga_dma_fe_fsm_const_ptr_t p_fsm )
{
    p_fsm->reg_fun_ptr.dma_writer_frame_write_on_write( p_fsm->cmn.isp_base, 0 );
}


void fpga_dma_fe_init( fpga_dma_fe_fsm_ptr_t p_fsm )
{
#if ISP_HAS_FPGA_WRAPPER
    LOG( LOG_DEBUG, "+++" );
    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_DFE_FRAME_END );
    fpga_dma_fe_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );

    p_fsm->active_frame = NULL;
    p_fsm->next_frame = NULL;
    p_fsm->initialized = 0;
    p_fsm->pause = 0;
    p_fsm->last_bank = 0;


    LOG( LOG_INFO, "ctx_id: %d.", p_fsm->cmn.ctx_id );

    if ( 0 == p_fsm->cmn.ctx_id ) {
        // read
        p_fsm->reg_fun_ptr.top_active_width_read = acamera_fpga_fe_isp1_top_active_width_read;
        p_fsm->reg_fun_ptr.top_active_height_read = acamera_fpga_fe_isp1_top_active_height_read;
        p_fsm->reg_fun_ptr.dma_writer_wbank_last_read = acamera_fpga_fe_isp1_dma_writer_wbank_last_read;
        p_fsm->reg_fun_ptr.dma_writer_wbase_last_read = acamera_fpga_fe_isp1_dma_writer_wbase_last_read;

        // write
        p_fsm->reg_fun_ptr.dma_writer_frame_write_on_write = acamera_fpga_fe_isp1_dma_writer_frame_write_on_write;
        p_fsm->reg_fun_ptr.top_active_height_dma_write = acamera_fpga_fe_isp1_top_active_height_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_dma_write = acamera_fpga_fe_isp1_top_active_width_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_fe_write = acamera_fpga_fe_isp1_top_active_width_fe_write;
        p_fsm->reg_fun_ptr.top_active_height_fe_write = acamera_fpga_fe_isp1_top_active_height_fe_write;
        p_fsm->reg_fun_ptr.dma_writer_format_write = acamera_fpga_fe_isp1_dma_writer_format_write;
        p_fsm->reg_fun_ptr.dma_writer_line_offset_write = acamera_fpga_fe_isp1_dma_writer_line_offset_write;

        p_fsm->reg_fun_ptr.dma_writer_bank0_base_write = acamera_fpga_fe_isp1_dma_writer_bank0_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank1_base_write = acamera_fpga_fe_isp1_dma_writer_bank1_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank2_base_write = acamera_fpga_fe_isp1_dma_writer_bank2_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank3_base_write = acamera_fpga_fe_isp1_dma_writer_bank3_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank4_base_write = acamera_fpga_fe_isp1_dma_writer_bank4_base_write;
        p_fsm->reg_fun_ptr.dma_writer_max_bank_write = acamera_fpga_fe_isp1_dma_writer_max_bank_write;

    } else if ( 1 == p_fsm->cmn.ctx_id ) {
        // read
        p_fsm->reg_fun_ptr.top_active_width_read = acamera_fpga_fe_isp2_top_active_width_read;
        p_fsm->reg_fun_ptr.top_active_height_read = acamera_fpga_fe_isp2_top_active_height_read;
        p_fsm->reg_fun_ptr.dma_writer_wbank_last_read = acamera_fpga_fe_isp2_dma_writer_wbank_last_read;
        p_fsm->reg_fun_ptr.dma_writer_wbase_last_read = acamera_fpga_fe_isp2_dma_writer_wbase_last_read;

        // write
        p_fsm->reg_fun_ptr.dma_writer_frame_write_on_write = acamera_fpga_fe_isp2_dma_writer_frame_write_on_write;
        p_fsm->reg_fun_ptr.top_active_height_dma_write = acamera_fpga_fe_isp2_top_active_height_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_dma_write = acamera_fpga_fe_isp2_top_active_width_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_fe_write = acamera_fpga_fe_isp2_top_active_width_fe_write;
        p_fsm->reg_fun_ptr.top_active_height_fe_write = acamera_fpga_fe_isp2_top_active_height_fe_write;
        p_fsm->reg_fun_ptr.dma_writer_format_write = acamera_fpga_fe_isp2_dma_writer_format_write;
        p_fsm->reg_fun_ptr.dma_writer_line_offset_write = acamera_fpga_fe_isp2_dma_writer_line_offset_write;

        p_fsm->reg_fun_ptr.dma_writer_bank0_base_write = acamera_fpga_fe_isp2_dma_writer_bank0_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank1_base_write = acamera_fpga_fe_isp2_dma_writer_bank1_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank2_base_write = acamera_fpga_fe_isp2_dma_writer_bank2_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank3_base_write = acamera_fpga_fe_isp2_dma_writer_bank3_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank4_base_write = acamera_fpga_fe_isp2_dma_writer_bank4_base_write;
        p_fsm->reg_fun_ptr.dma_writer_max_bank_write = acamera_fpga_fe_isp2_dma_writer_max_bank_write;
    } else if ( 2 == p_fsm->cmn.ctx_id ) {
        // read
        p_fsm->reg_fun_ptr.top_active_width_read = acamera_fpga_fe_isp3_top_active_width_read;
        p_fsm->reg_fun_ptr.top_active_height_read = acamera_fpga_fe_isp3_top_active_height_read;
        p_fsm->reg_fun_ptr.dma_writer_wbank_last_read = acamera_fpga_fe_isp3_dma_writer_wbank_last_read;
        p_fsm->reg_fun_ptr.dma_writer_wbase_last_read = acamera_fpga_fe_isp3_dma_writer_wbase_last_read;

        // write
        p_fsm->reg_fun_ptr.dma_writer_frame_write_on_write = acamera_fpga_fe_isp3_dma_writer_frame_write_on_write;
        p_fsm->reg_fun_ptr.top_active_height_dma_write = acamera_fpga_fe_isp3_top_active_height_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_dma_write = acamera_fpga_fe_isp3_top_active_width_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_fe_write = acamera_fpga_fe_isp3_top_active_width_fe_write;
        p_fsm->reg_fun_ptr.top_active_height_fe_write = acamera_fpga_fe_isp3_top_active_height_fe_write;
        p_fsm->reg_fun_ptr.dma_writer_format_write = acamera_fpga_fe_isp3_dma_writer_format_write;
        p_fsm->reg_fun_ptr.dma_writer_line_offset_write = acamera_fpga_fe_isp3_dma_writer_line_offset_write;

        p_fsm->reg_fun_ptr.dma_writer_bank0_base_write = acamera_fpga_fe_isp3_dma_writer_bank0_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank1_base_write = acamera_fpga_fe_isp3_dma_writer_bank1_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank2_base_write = acamera_fpga_fe_isp3_dma_writer_bank2_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank3_base_write = acamera_fpga_fe_isp3_dma_writer_bank3_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank4_base_write = acamera_fpga_fe_isp3_dma_writer_bank4_base_write;
        p_fsm->reg_fun_ptr.dma_writer_max_bank_write = acamera_fpga_fe_isp3_dma_writer_max_bank_write;
    } else if ( 3 == p_fsm->cmn.ctx_id ) {
        // read
        p_fsm->reg_fun_ptr.top_active_width_read = acamera_fpga_fe_isp4_top_active_width_read;
        p_fsm->reg_fun_ptr.top_active_height_read = acamera_fpga_fe_isp4_top_active_height_read;
        p_fsm->reg_fun_ptr.dma_writer_wbank_last_read = acamera_fpga_fe_isp4_dma_writer_wbank_last_read;
        p_fsm->reg_fun_ptr.dma_writer_wbase_last_read = acamera_fpga_fe_isp4_dma_writer_wbase_last_read;

        // write
        p_fsm->reg_fun_ptr.dma_writer_frame_write_on_write = acamera_fpga_fe_isp4_dma_writer_frame_write_on_write;
        p_fsm->reg_fun_ptr.top_active_height_dma_write = acamera_fpga_fe_isp4_top_active_height_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_dma_write = acamera_fpga_fe_isp4_top_active_width_dma_write;
        p_fsm->reg_fun_ptr.top_active_width_fe_write = acamera_fpga_fe_isp4_top_active_width_fe_write;
        p_fsm->reg_fun_ptr.top_active_height_fe_write = acamera_fpga_fe_isp4_top_active_height_fe_write;
        p_fsm->reg_fun_ptr.dma_writer_format_write = acamera_fpga_fe_isp4_dma_writer_format_write;
        p_fsm->reg_fun_ptr.dma_writer_line_offset_write = acamera_fpga_fe_isp4_dma_writer_line_offset_write;

        p_fsm->reg_fun_ptr.dma_writer_bank0_base_write = acamera_fpga_fe_isp4_dma_writer_bank0_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank1_base_write = acamera_fpga_fe_isp4_dma_writer_bank1_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank2_base_write = acamera_fpga_fe_isp4_dma_writer_bank2_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank3_base_write = acamera_fpga_fe_isp4_dma_writer_bank3_base_write;
        p_fsm->reg_fun_ptr.dma_writer_bank4_base_write = acamera_fpga_fe_isp4_dma_writer_bank4_base_write;
        p_fsm->reg_fun_ptr.dma_writer_max_bank_write = acamera_fpga_fe_isp4_dma_writer_max_bank_write;
    }

    acamera_settings *p_settings = &( ACAMERA_FSM2CTX_PTR( p_fsm )->settings );
    void *virt_addr;
    uint64_t dma_addr;
    int i;

    for ( i = 0; i < p_settings->dfe_frames_number; i++ ) {
        virt_addr = p_settings->callback_dma_alloc_coherent(
            p_fsm->cmn.ctx_id, p_settings->dfe_frames[i].size, &dma_addr );
        if ( !virt_addr ) {
            LOG( LOG_ERR, "Alloc DFE frames failed." );
            return;
        }

        p_settings->dfe_frames[i].address = (uint32_t)dma_addr;
        p_settings->dfe_frames[i].virt_addr = virt_addr;
    }

    LOG( LOG_DEBUG, "def frames..." );
    for ( p_fsm->frame_number = 0; p_fsm->frame_number < ACAMERA_FSM2CTX_PTR( p_fsm )->settings.dfe_frames_number && p_fsm->frame_number < DFE_MAX_BANK_NUM; p_fsm->frame_number++ ) {
        p_fsm->frame_list[p_fsm->frame_number] = ACAMERA_FSM2CTX_PTR( p_fsm )->settings.dfe_frames[p_fsm->frame_number];
        p_fsm->frame_list[p_fsm->frame_number].status = dfe_frame_free;
    }

    LOG( LOG_INFO, "frame_number: %d.", p_fsm->frame_number );

    if ( p_fsm->frame_number > 0 ) {
        LOG( LOG_DEBUG, "p_fsm->cmn.isp_base: 0x%x, ptr: %p, %p.", p_fsm->cmn.isp_base, p_fsm->reg_fun_ptr.top_active_width_read, p_fsm->reg_fun_ptr.top_active_height_read );
        uint16_t top_active_width = p_fsm->reg_fun_ptr.top_active_width_read( p_fsm->cmn.isp_base );
        uint16_t top_active_height = p_fsm->reg_fun_ptr.top_active_height_read( p_fsm->cmn.isp_base );

        LOG( LOG_DEBUG, "top_active_width: %d, top_active_height: %d.", top_active_width, top_active_height );

        // fe writer
        p_fsm->reg_fun_ptr.top_active_width_dma_write( p_fsm->cmn.isp_base, top_active_width );
        p_fsm->reg_fun_ptr.top_active_height_dma_write( p_fsm->cmn.isp_base, top_active_height );
        // front-end
        p_fsm->reg_fun_ptr.top_active_width_fe_write( p_fsm->cmn.isp_base, top_active_width );
        p_fsm->reg_fun_ptr.top_active_height_fe_write( p_fsm->cmn.isp_base, top_active_height );


        LOG( LOG_DEBUG, "type: %d.", p_fsm->frame_list[0].type );
        //set the parameters based only of first frame
        //acamera_isp_dma_writer_base_mode_write( p_fsm->cmn.isp_base, p_fsm->frame_list[0].type ) ;
        //changed to format
        p_fsm->reg_fun_ptr.dma_writer_format_write( p_fsm->cmn.isp_base, p_fsm->frame_list[0].type );
        p_fsm->reg_fun_ptr.dma_writer_line_offset_write( p_fsm->cmn.isp_base, acamera_line_offset( top_active_width, _get_pixel_width( p_fsm->frame_list[0].type ) ) );
        LOG( LOG_DEBUG, "acamera_isp_dma_writer_line_offset_write b:%x l:%d t:%d w:%d", p_fsm->cmn.isp_base, p_fsm->frame_list[0].line_offset, p_fsm->frame_list[0].type, top_active_width );

        p_fsm->reg_fun_ptr.dma_writer_bank0_base_write( p_fsm->cmn.isp_base, p_fsm->frame_list[0].address );
        if ( p_fsm->frame_number > 1 )
            p_fsm->reg_fun_ptr.dma_writer_bank1_base_write( p_fsm->cmn.isp_base, p_fsm->frame_list[1].address );
        if ( p_fsm->frame_number > 2 )
            p_fsm->reg_fun_ptr.dma_writer_bank2_base_write( p_fsm->cmn.isp_base, p_fsm->frame_list[2].address );
        if ( p_fsm->frame_number > 3 )
            p_fsm->reg_fun_ptr.dma_writer_bank3_base_write( p_fsm->cmn.isp_base, p_fsm->frame_list[3].address );
        if ( p_fsm->frame_number > 4 )
            p_fsm->reg_fun_ptr.dma_writer_bank4_base_write( p_fsm->cmn.isp_base, p_fsm->frame_list[4].address );
        p_fsm->reg_fun_ptr.dma_writer_max_bank_write( p_fsm->cmn.isp_base, p_fsm->frame_number - 1 );
        p_fsm->reg_fun_ptr.dma_writer_frame_write_on_write( p_fsm->cmn.isp_base, 1 );

        p_fsm->initialized = 1;
        LOG( LOG_DEBUG, "--- 2" );
        return;
    }

    // do not write any frames because we do not have any input buffers on this stage
    fpga_dma_fe_stop( p_fsm );

    LOG( LOG_DEBUG, "---" );
#endif
}

void fpga_dma_fe_deinit( fpga_dma_fe_fsm_ptr_t p_fsm )
{
#if ISP_HAS_FPGA_WRAPPER
    acamera_settings *p_settings = &( ACAMERA_FSM2CTX_PTR( p_fsm )->settings );
    void *virt_addr;
    uint64_t dma_addr;
    size_t size;
    int i;

    for ( i = 0; i < p_settings->dfe_frames_number; i++ ) {
        virt_addr = p_settings->dfe_frames[i].virt_addr;
        dma_addr = p_settings->dfe_frames[i].address;
        size = p_settings->dfe_frames[i].size;

        p_settings->callback_dma_free_coherent( p_fsm->cmn.ctx_id, size, virt_addr, dma_addr );
    }
#endif
}

void fpga_dma_fe_fsm_process_interrupt( fpga_dma_fe_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    if ( p_fsm->initialized == 1 && p_fsm->pause == 0 ) {
        if ( irq_event == ACAMERA_IRQ_DFE_FRAME_END ) {
            //No need to update frames manually but we can update the status here
            uint8_t last_bank = p_fsm->reg_fun_ptr.dma_writer_wbank_last_read( p_fsm->cmn.isp_base );
            if ( last_bank < p_fsm->frame_number ) {
                ( (fpga_dma_fe_fsm_ptr_t)p_fsm )->last_bank = last_bank;
                ( (fpga_dma_fe_fsm_ptr_t)p_fsm )->frame_list[last_bank].status = dfe_frame_filled;
            }
        }
    } else {
        // error - fsm was not initialized
        LOG( LOG_ERR, "Error. DMA FE FSM was not initialized properly" );
    }
}


int32_t fpga_dma_fe_get_oldest_frame( fpga_dma_fe_fsm_ptr_t p_fsm, aframe_t **frame )
{
    uint32_t last_base = p_fsm->reg_fun_ptr.dma_writer_wbase_last_read( p_fsm->cmn.isp_base );

    *frame = &p_fsm->frame_list[0];
    p_fsm->frame_list[0].address = last_base;
    p_fsm->frame_list[0].status = dfe_frame_filled;
    return 0;
}
