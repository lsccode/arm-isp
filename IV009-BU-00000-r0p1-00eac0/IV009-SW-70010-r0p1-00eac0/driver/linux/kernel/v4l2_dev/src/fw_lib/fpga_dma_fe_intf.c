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

#include "acamera_fw.h"
#include "fpga_dma_fe_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_FPGA_DMA_FE
#endif

/* Use static memory here to make it cross-platform */
static fpga_dma_fe_fsm_t dma_fe_fsm_ctxs[FIRMWARE_CONTEXT_NUMBER];

fsm_common_t *fpga_dma_fe_get_fsm_common( uint8_t ctx_id )
{
    fpga_dma_fe_fsm_t *p_fsm_ctx = NULL;

    if ( ctx_id >= FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_CRIT, "Invalid ctx_id: %d, greater than max: %d.", ctx_id, FIRMWARE_CONTEXT_NUMBER - 1 );
        return NULL;
    }

    p_fsm_ctx = &dma_fe_fsm_ctxs[ctx_id];

    p_fsm_ctx->cmn.ctx_id = ctx_id;
    p_fsm_ctx->cmn.p_fsm = (void *)p_fsm_ctx;

    p_fsm_ctx->cmn.ops.init = fpga_dma_fe_fsm_init;
    p_fsm_ctx->cmn.ops.deinit = fpga_dma_fe_fsm_deinit;
    p_fsm_ctx->cmn.ops.run = NULL;
    p_fsm_ctx->cmn.ops.set_param = NULL;
    p_fsm_ctx->cmn.ops.get_param = fpga_dma_fe_fsm_get_param;
    p_fsm_ctx->cmn.ops.proc_event = NULL;
    p_fsm_ctx->cmn.ops.proc_interrupt = (FUN_PTR_PROC_INT)fpga_dma_fe_fsm_process_interrupt;

    return &( p_fsm_ctx->cmn );
}
