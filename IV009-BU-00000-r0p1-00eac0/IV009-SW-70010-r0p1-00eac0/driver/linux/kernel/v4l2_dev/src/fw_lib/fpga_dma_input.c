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

#include "acamera_types.h"
#include "acamera_fw.h"
#include "acamera.h"
#include "system_log.h"
#include "acamera_isp_config.h"

#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_config.h"
#include "acamera_fpga_fe_isp1_config.h"
#include "acamera_fpga_fe_isp2_config.h"
#include "acamera_fpga_fe_isp_config.h"


#include "acamera_fpga_fe_isp_config.h"
#include "dfe_control_api.h"


// extern int32_t fpga_dma_fe_get_oldest_frame( fpga_dma_fe_fsm_ptr_t p_fsm, aframe_t** frame ) ;
static void fpga_dma_init_registers( void )
{
    // fe_isp1 input port
    acamera_fpga_fe_isp1_input_port_preset_write( 0, 15 );
    acamera_fpga_fe_isp1_input_port_aclg_window0_write( 0, 1 );
    acamera_fpga_fe_isp1_input_port_aclg_hsync_write( 0, 1 );
    acamera_fpga_fe_isp1_input_port_aclg_window2_write( 0, 1 );
    acamera_fpga_fe_isp1_input_port_fieldg_vsync_write( 0, 1 );
    acamera_fpga_fe_isp1_input_port_hc_start0_write( 0, 48 );
    acamera_fpga_fe_isp1_input_port_hc_size0_write( 0, 1920 );
    acamera_fpga_fe_isp1_input_port_mode_request_write( 0, 1 );

    acamera_fpga_fe_isp1_dma_writer_half_irate_write( 0, 1 );
    acamera_fpga_fe_isp1_dma_writer_axi_port_enable_write( 0, 1 );

    // fe_isp2 input port
    acamera_fpga_fe_isp2_input_port_preset_write( 0, 15 );
    acamera_fpga_fe_isp2_input_port_aclg_window0_write( 0, 1 );
    acamera_fpga_fe_isp2_input_port_aclg_hsync_write( 0, 1 );
    acamera_fpga_fe_isp2_input_port_aclg_window2_write( 0, 1 );
    acamera_fpga_fe_isp2_input_port_fieldg_vsync_write( 0, 1 );
    acamera_fpga_fe_isp2_input_port_hc_start0_write( 0, 48 );
    acamera_fpga_fe_isp2_input_port_hc_size0_write( 0, 1920 );
    acamera_fpga_fe_isp2_input_port_mode_request_write( 0, 1 );

    acamera_fpga_fe_isp2_dma_writer_half_irate_write( 0, 1 );
    acamera_fpga_fe_isp2_dma_writer_axi_port_enable_write( 0, 1 );

    // interrupts
    acamera_fpga_fe_isp_interrupts_interrupt0_source_write( 0, 32 );
    acamera_fpga_fe_isp_interrupts_interrupt1_source_write( 0, 33 );
    acamera_fpga_fe_isp_interrupts_interrupt2_source_write( 0, 34 );
    acamera_fpga_fe_isp_interrupts_interrupt3_source_write( 0, 35 );

    // Input source
    acamera_fpga_fe_isp_top_input_source_write( 0, 1 );

    acamera_fpga_fe_isp_dma_input_pre_fetch_dly_write( 0, 1920 );
    acamera_fpga_fe_isp_dma_input_interline_dly_write( 0, 1920 );
    acamera_fpga_fe_isp_dma_input_field_dly_write( 0, 1920 );
    acamera_fpga_fe_isp_dma_input_line_offset_write( 0, 3840 );
    acamera_fpga_fe_isp_dma_input_base_mode_write( 0, 6 );
    acamera_fpga_fe_isp_dma_input_axi_port_enable_write( 0, 1 );
}

int32_t fpga_dma_input_init( acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    g_fw->dma_input.cur_ctx = -1;
    g_fw->dma_input.next_ctx = -1;
    g_fw->dma_input.wait_isp_frame_end = 0;
    g_fw->dma_input.active_frame = NULL;
    g_fw->dma_input.freeze_ctx = 0;
    g_fw->dma_input.mask_ctx = 0;
    g_fw->dma_input.last_ctx = 0;
    // use the same type as DFE. Suppose that they all have the same format.
    acamera_fpga_fe_isp_dma_input_base_mode_write( 0, g_fw->fw_ctx[0].settings.dfe_frames[0].type );

    // #if ISP_HAS_RAW_CB
    //     g_fw->dma_input.raw_cb = NULL;
    //     LOG ( LOG_INFO, "raw callback reset to NULL (%p)", g_fw->dma_input.raw_cb );
    // #endif

    // init interrupt routines
    g_fw->dma_input.fpga_isp_interrupt_source_read[0] = acamera_fpga_fe_isp_interrupts_interrupt0_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[1] = acamera_fpga_fe_isp_interrupts_interrupt1_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[2] = acamera_fpga_fe_isp_interrupts_interrupt2_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[3] = acamera_fpga_fe_isp_interrupts_interrupt3_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[4] = acamera_fpga_fe_isp_interrupts_interrupt4_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[5] = acamera_fpga_fe_isp_interrupts_interrupt5_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[6] = acamera_fpga_fe_isp_interrupts_interrupt6_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[7] = acamera_fpga_fe_isp_interrupts_interrupt7_source_read;
#if ISP_INTERRUPT_EVENT_NONES_COUNT == 16
    g_fw->dma_input.fpga_isp_interrupt_source_read[8] = acamera_fpga_fe_isp_interrupts_interrupt8_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[9] = acamera_fpga_fe_isp_interrupts_interrupt9_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[10] = acamera_fpga_fe_isp_interrupts_interrupt10_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[11] = acamera_fpga_fe_isp_interrupts_interrupt11_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[12] = acamera_fpga_fe_isp_interrupts_interrupt12_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[13] = acamera_fpga_fe_isp_interrupts_interrupt13_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[14] = acamera_fpga_fe_isp_interrupts_interrupt14_source_read;
    g_fw->dma_input.fpga_isp_interrupt_source_read[15] = acamera_fpga_fe_isp_interrupts_interrupt15_source_read;
#endif

    fpga_dma_init_registers();

    return result;
}

static int32_t dma_input_process_frame( acamera_firmware_t *g_fw, aframe_t *frame )
{
    int32_t result = 0;
    static uint32_t processed[4];
    if ( frame != NULL && g_fw->dma_input.cur_ctx != -1 ) {

        // #if defined(ISP_HAS_DMA_WRITER_FSM)
        //         //configure DMA WRITER here for the right context
        //         dma_writer_update_address_interrupt(&g_fw->fw_ctx[ g_fw->dma_input.cur_ctx ].isp.dma_writer_fsm,APICAL_IRQ_FRAME_WRITER_FR);
        //         dma_writer_update_address_interrupt(&g_fw->fw_ctx[ g_fw->dma_input.cur_ctx ].isp.dma_writer_fsm,APICAL_IRQ_FRAME_WRITER_DS);
        // #endif
        acamera_fpga_fe_isp_dma_input_rbase_write( 0, frame->address );
        acamera_fpga_fe_isp_dma_input_context_id_write( 0, g_fw->dma_input.cur_ctx );
        acamera_fpga_fe_isp_dma_input_frame_request_write( 0, 0 );
        acamera_fpga_fe_isp_dma_input_frame_request_write( 0, 1 );
        processed[g_fw->dma_input.cur_ctx]++;

        // update_mesh_shading( g_fw ) ;

        LOG( LOG_INFO, "cur_ctx: %u, DMA Input trigger frame request for frame 0x%x, dma_input_frame_request_read: %u ",
             (unsigned int)g_fw->dma_input.cur_ctx,
             (unsigned int)frame->address,
             (unsigned int)acamera_fpga_fe_isp_dma_input_frame_request_read( 0 ) );
        // #if ISP_HAS_RAW_CB
        //         if(g_fw->dma_input.raw_cb != NULL) {
        //             // Todo: delivering empty metadata, later it can be simply frame_id
        //             tframe_t tframe;
        //             metadata_t meta;

        //             tframe.primary = *frame;
        //             meta.frame_id = g_fw->fw_ctx[g_fw->dma_input.cur_ctx].isp_frame_counter_raw;

        //             g_fw->dma_input.raw_cb(&g_fw->fw_ctx[ g_fw->dma_input.cur_ctx ], &tframe, &meta);
        //         }
        // #endif
    } else {
        result = -1;
        LOG( LOG_NOTICE, "DMA Input failed to update - input frame = %p, cur_ctx = %d",
             frame, (int)g_fw->dma_input.cur_ctx );
    }
    return result;
}


// void freeze_dma_input_context(acamera_firmware_t* g_fw,uint32_t ctx_num){
//     if(ctx_num < get_context_number() ){
//         g_fw->dma_input.freeze_ctx=1;
//     }else{
//         g_fw->dma_input.freeze_ctx=0;
//     }

// }

static int32_t find_next_ctx( acamera_firmware_t *g_fw, int32_t skip_ctx )
{
    // find the proper context which has frames and can be processed next
    // the skip_ctx will be skipped
    int32_t result = -1;
    int32_t idx = 1;
    for ( idx = 0; idx < g_fw->context_number; idx++ ) {
        uint32_t last_id = ( g_fw->dma_input.last_ctx + idx + 1 ) % g_fw->context_number;
        uint32_t tmp_id = g_fw->dma_input.mask_ctx & ( 1 << last_id );
        if ( tmp_id && last_id != skip_ctx ) {
            result = last_id;
            g_fw->dma_input.last_ctx = last_id;
            break;
        }
    }
    return result;
}


static int dma_input_update_context( acamera_firmware_t *g_fw )
{
    int32_t ret = -1;
    int32_t ctx1 = g_fw->dma_input.cur_ctx;
    int32_t ctx2 = g_fw->dma_input.next_ctx;

    if ( ( ctx1 == -1 && ctx2 == -1 ) || g_fw->context_number == 1 ) { //single context single frame fix
        ctx1 = find_next_ctx( g_fw, -1 );
        LOG( LOG_INFO, "found context1 = %d", (int)ctx1 );
    } else if ( ctx1 != -1 && ctx2 != -1 ) {
        ctx1 = ctx2;
    }

    if ( g_fw->context_number > 1 ) {
        // multi context mode. need the next context for the temper buffer
        ctx2 = find_next_ctx( g_fw, ctx1 );
    } else {
        // single context mode - use the same context all the time
        ctx2 = ctx1;
    }

    if ( ctx1 != -1 && ctx2 != -1 ) {
        g_fw->dma_input.cur_ctx = ctx1;
        g_fw->dma_input.next_ctx = ctx2;
        ret = 0;
    } else {
        ret = -1;
    }

    LOG( LOG_INFO, "ret: %d, ctx1: %d, ctx2: %d.", ret, ctx1, ctx2 );
    return ret;
}

int32_t fpga_dma_input_last_context( acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    if ( g_fw->dma_input.last_ctx != -1 ) {
        result = g_fw->dma_input.last_ctx;
    } else {
        result = 0;
    }
    return result;
}

int32_t fpga_dma_input_current_context( acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    if ( g_fw->dma_input.cur_ctx != -1 ) {
        result = g_fw->dma_input.cur_ctx;
    } else {
        result = 0;
    }
    return result;
}

int32_t fpga_dma_input_next_context( acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    if ( g_fw->dma_input.next_ctx != -1 ) {
        result = g_fw->dma_input.next_ctx;
    } else {
        result = 0;
    }
    return result;
}

int32_t dma_input_process( acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    aframe_t *frame = NULL;
    if ( g_fw->dma_input.cur_ctx != -1 ) {
        fsm_param_dma_frame_t fsm_param;
        fsm_param.p_frame = &frame;
        result = acamera_fsm_mgr_get_param( &g_fw->fw_ctx[g_fw->dma_input.cur_ctx].fsm_mgr, FSM_PARAM_GET_DMA_FE_FRAME, NULL, 0, &fsm_param, sizeof( fsm_param ) );
    } else {
        result = -1;
    }

    if ( result != -1 && frame != NULL ) {
        if ( frame->status == dfe_frame_filled ) {
            LOG( LOG_INFO, "Process %d context, frame 0x%x", (int)g_fw->dma_input.cur_ctx, (unsigned int)frame->address );
            result = dma_input_process_frame( g_fw, frame );
            g_fw->dma_input.active_frame = frame;
            g_fw->dma_input.wait_isp_frame_end = 1;
            if ( result != -1 ) {
            } else {
                result = -1;
                LOG( LOG_ERR, "Failed to process frame by dma input" );
            }
        } else {
            result = -1;
            LOG( LOG_ERR, "Invalid frame status %d. Expected status is filled ", (int)frame->status );
        }
    } else {
        g_fw->dma_input.wait_isp_frame_end = 0;
        result = -1;
        LOG( LOG_ERR, "Failed to get the frame to process by dma input for context %d", (int)g_fw->dma_input.cur_ctx );
    }
    return result;
}


int32_t start_frame( acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    if ( dma_input_update_context( g_fw ) != -1 ) {
        result = dma_input_process( g_fw );
        if ( result != -1 ) {
            //free mask
            g_fw->dma_input.mask_ctx &= ~( 1 << g_fw->dma_input.cur_ctx );
            g_fw->dma_input.wait_isp_frame_end = 1;
        } else {
            result = -1;
            g_fw->dma_input.wait_isp_frame_end = 0;
            LOG( LOG_ERR, "Failed to start ISP on ISP-FE" );
        }
    } else {
        LOG( LOG_INFO, "Cannot find a second context to process on ISP-FE:mask %d", (int)g_fw->dma_input.mask_ctx );
        g_fw->dma_input.wait_isp_frame_end = 0;
    }
    return result;
}

void fpga_dma_input_interrupt( acamera_firmware_t *g_fw, uint32_t irq_event, uint32_t ctx_id )
{
    int32_t result = 0;
    if ( irq_event == ACAMERA_IRQ_FRAME_END ) {
        if ( g_fw->dma_input.wait_isp_frame_end == 1 ) {
            if ( g_fw->dma_input.active_frame != NULL ) {
                g_fw->dma_input.active_frame->status = dfe_frame_free;
                g_fw->dma_input.active_frame = NULL;
                g_fw->dma_input.wait_isp_frame_end = 0;

                result = start_frame( g_fw );
                if ( result != 0 ) {
                    LOG( LOG_ERR, "Failed to start frame " );
                }

            } else {
                LOG( LOG_ERR, "No frame was assigned with dma input context" );
            }

        } else {
            LOG( LOG_ERR, "Error: Not expected ISP-Frame End" );
        }
    } else if ( irq_event == ACAMERA_IRQ_DFE_FRAME_END ) {
        g_fw->dma_input.mask_ctx |= ( 1 << ctx_id );
        LOG( LOG_INFO, "New DMA-FE event for the context %d. mask %d", (int)ctx_id, (int)g_fw->dma_input.mask_ctx );
        if ( g_fw->dma_input.wait_isp_frame_end == 0 ) {
            // the isp processing has not been started
            // initiate it on dma front-end
            result = start_frame( g_fw );
            if ( result != 0 ) {
                LOG( LOG_ERR, "Failed to start frame " );
            }
        } else {
            LOG( LOG_INFO, "skip due to dma_input.wait_isp_frame_end: %d", (int)g_fw->dma_input.wait_isp_frame_end );
        }
    }
}

// #if ISP_HAS_RAW_CB
// int32_t dma_input_regist_callback( acamera_firmware_t* g_fw, raw_callback_t cb )
// {
//     if ( g_fw == NULL) {
//         LOG( LOG_ERR, "g_fw is NULL");
//         return -EINVAL;
//     }

//     g_fw->dma_input.raw_cb = cb;
//     LOG ( LOG_INFO, "raw callback registered (%p)", g_fw->dma_input.raw_cb );

//     return 0;
// }
// #endif

#endif
