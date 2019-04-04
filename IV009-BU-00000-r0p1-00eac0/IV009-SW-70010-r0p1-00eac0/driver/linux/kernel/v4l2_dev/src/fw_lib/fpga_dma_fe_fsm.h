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

#if !defined( __DMA_FE_FSM_H__ )
#define __DMA_FE_FSM_H__



typedef struct _fpga_dma_fe_fsm_t fpga_dma_fe_fsm_t;
typedef struct _fpga_dma_fe_fsm_t *fpga_dma_fe_fsm_ptr_t;
typedef const struct _fpga_dma_fe_fsm_t *fpga_dma_fe_fsm_const_ptr_t;

#include "acamera_fw.h"
#include "acamera_firmware_config.h"
#include "acamera_firmware_api.h"
#include "dfe_control_api.h"
#define DFE_MAX_BANK_NUM 5

void fpga_dma_fe_init( fpga_dma_fe_fsm_ptr_t p_fsm );
void fpga_dma_fe_deinit( fpga_dma_fe_fsm_ptr_t p_fsm );
void dma_fe_apply_config( fpga_dma_fe_fsm_ptr_t p_fsm );
void dma_fe_apply_layout( fpga_dma_fe_fsm_ptr_t p_fsm, const char *layout );
int32_t fpga_dma_fe_get_oldest_frame( fpga_dma_fe_fsm_ptr_t p_fsm, aframe_t **frame );

typedef uint8_t ( *FPGA_REG_READ_UINT8_FUN_PTR )( uintptr_t base );
typedef uint16_t ( *FPGA_REG_READ_UINT16_FUN_PTR )( uintptr_t base );
typedef uint32_t ( *FPGA_REG_READ_UINT32_FUN_PTR )( uintptr_t base );

typedef void ( *FPGA_REG_WRITE_UINT8_FUN_PTR )( uintptr_t base, uint8_t data );
typedef void ( *FPGA_REG_WRITE_UINT16_FUN_PTR )( uintptr_t base, uint16_t data );
typedef void ( *FPGA_REG_WRITE_UINT32_FUN_PTR )( uintptr_t base, uint32_t data );

typedef struct _fpga_reg_fun_ptr_ {
    // read
    FPGA_REG_READ_UINT16_FUN_PTR top_active_width_read;
    FPGA_REG_READ_UINT16_FUN_PTR top_active_height_read;
    FPGA_REG_READ_UINT8_FUN_PTR dma_writer_wbank_last_read;
    FPGA_REG_READ_UINT32_FUN_PTR dma_writer_wbase_last_read;

    // write
    FPGA_REG_WRITE_UINT8_FUN_PTR dma_writer_frame_write_on_write;
    FPGA_REG_WRITE_UINT16_FUN_PTR top_active_width_dma_write;
    FPGA_REG_WRITE_UINT16_FUN_PTR top_active_height_dma_write;
    FPGA_REG_WRITE_UINT16_FUN_PTR top_active_width_fe_write;
    FPGA_REG_WRITE_UINT16_FUN_PTR top_active_height_fe_write;
    FPGA_REG_WRITE_UINT8_FUN_PTR dma_writer_format_write;
    FPGA_REG_WRITE_UINT32_FUN_PTR dma_writer_line_offset_write;

    FPGA_REG_WRITE_UINT32_FUN_PTR dma_writer_bank0_base_write;
    FPGA_REG_WRITE_UINT32_FUN_PTR dma_writer_bank1_base_write;
    FPGA_REG_WRITE_UINT32_FUN_PTR dma_writer_bank2_base_write;
    FPGA_REG_WRITE_UINT32_FUN_PTR dma_writer_bank3_base_write;
    FPGA_REG_WRITE_UINT32_FUN_PTR dma_writer_bank4_base_write;
    FPGA_REG_WRITE_UINT8_FUN_PTR dma_writer_max_bank_write;

} fpga_reg_func_ptr_t;


struct _fpga_dma_fe_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint32_t initialized;
    uint8_t pause;
    uint64_t work_time;
    uint32_t start_time;
    aframe_t *active_frame;
    aframe_t *next_frame;
    aframe_t frame_list[DFE_MAX_BANK_NUM];
    uint8_t frame_number;
    uint8_t last_bank;
    fpga_reg_func_ptr_t reg_fun_ptr;
};


void fpga_dma_fe_fsm_clear( fpga_dma_fe_fsm_ptr_t p_fsm );
void fpga_dma_fe_fsm_init( void *fsm, fsm_init_param_t *init_param );
void fpga_dma_fe_fsm_deinit( void *fsm );
int fpga_dma_fe_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

void fpga_dma_fe_fsm_process_interrupt( fpga_dma_fe_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void fpga_dma_fe_request_interrupt( fpga_dma_fe_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __DMA_FE_FSM_H__ */
