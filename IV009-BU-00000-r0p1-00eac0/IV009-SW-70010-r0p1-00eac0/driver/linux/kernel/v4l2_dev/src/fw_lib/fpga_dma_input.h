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

#ifndef __DMA_INPUT_H__
#define __DMA_INPUT_H__

// #if ISP_HAS_RAW_CB
// typedef void (*raw_callback_t)( void* ctx, tframe_t *tframe, const metadata_t *metadata);
// #endif

// irq routines

typedef uint8_t ( *REG_INT_READ_FUN_PTR )( uintptr_t base );


typedef struct _dma_input_t {
    aframe_t *active_frame;
    volatile uint32_t wait_isp_frame_end;
    volatile uint32_t cur_ctx;
    volatile uint32_t next_ctx;
    uint32_t last_ctx;
    uint32_t freeze_ctx;
    volatile uint8_t mask_ctx;

    REG_INT_READ_FUN_PTR fpga_isp_interrupt_source_read[ISP_INTERRUPT_EVENT_NONES_COUNT];
    // #if ISP_HAS_RAW_CB
    //   raw_callback_t raw_cb;
    // #endif
} dma_input_t;

int32_t fpga_dma_input_init( acamera_firmware_t *g_fw );

void fpga_dma_input_interrupt( acamera_firmware_t *g_fw, uint32_t irq_event, uint32_t ctx_id );

int32_t fpga_dma_input_last_context( acamera_firmware_t *g_fw );
int32_t fpga_dma_input_current_context( acamera_firmware_t *g_fw );
int32_t fpga_dma_input_next_context( acamera_firmware_t *g_fw );

int32_t dma_input_process( acamera_firmware_t *g_fw );

// #if ISP_HAS_RAW_CB
// int32_t dma_input_regist_callback( acamera_firmware_t* g_fw, raw_callback_t cb );
// #endif

#endif // __DMA_INPUT_H__
