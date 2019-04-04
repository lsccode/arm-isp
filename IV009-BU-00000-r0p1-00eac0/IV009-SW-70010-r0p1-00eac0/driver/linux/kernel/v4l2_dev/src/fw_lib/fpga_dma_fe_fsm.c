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

void fpga_dma_fe_fsm_clear( fpga_dma_fe_fsm_t *p_fsm )
{
    p_fsm->initialized = 0;
    p_fsm->pause = 0;
    p_fsm->work_time = 0;
    p_fsm->start_time = 0;
    p_fsm->active_frame = NULL;
    p_fsm->next_frame = NULL;
    p_fsm->frame_number = 0;
    p_fsm->last_bank = 0;
}

void fpga_dma_fe_request_interrupt( fpga_dma_fe_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void fpga_dma_fe_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    fpga_dma_fe_fsm_t *p_fsm = (fpga_dma_fe_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    fpga_dma_fe_fsm_clear( p_fsm );

    fpga_dma_fe_init( p_fsm );
}

void fpga_dma_fe_fsm_deinit( void *fsm )
{
    fpga_dma_fe_fsm_t *p_fsm = (fpga_dma_fe_fsm_t *)fsm;
    fpga_dma_fe_deinit( p_fsm );
}

int fpga_dma_fe_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    fpga_dma_fe_fsm_t *p_fsm = (fpga_dma_fe_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_DMA_FE_FRAME: {
        fsm_param_dma_frame_t *p_dma_frame = NULL;

        if ( output_size != sizeof( fsm_param_dma_frame_t ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_dma_frame = (fsm_param_dma_frame_t *)output;

        fpga_dma_fe_get_oldest_frame( p_fsm, p_dma_frame->p_frame );

        break;
    }
    default:
        rc = -1;
        break;
    }

    return rc;
}
