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

#if !defined( __DMA_WRITER_FSM_H__ )
#define __DMA_WRITER_FSM_H__



typedef struct _dma_writer_fsm_t dma_writer_fsm_t;
typedef struct _dma_writer_fsm_t *dma_writer_fsm_ptr_t;
typedef const struct _dma_writer_fsm_t *dma_writer_fsm_const_ptr_t;

enum _dma_writer_state_t {
    dma_writer_state_wait_for_sensor,
    dma_writer_state_frame_processing,
    dma_writer_state_frame_processing_FR_finished,
    dma_writer_state_frame_processing_wait_events,
    dma_writer_state_frame_processing_DS_finished,
    dma_writer_state_frame_processing_prepare_metadata,
    dma_writer_state_frame_processing_event_processed,
    dma_writer_state_frame_processing_init,
    dma_writer_state_frame_processing_DS2_finished,
    dma_writer_state_frame_processing_buf_reinit,
    dma_writer_state_invalid,
    next_to_dma_writer_state_frame_processing = dma_writer_state_invalid
};

typedef enum _dma_writer_state_t dma_writer_state_t;

#include "acamera.h"
#include "acamera_command_api.h"

void frame_buffer_initialize( dma_writer_fsm_ptr_t p_fsm );
void dma_writer_deinit( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_prepare_metadata( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_fr_finished( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_ds_finished( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_check_and_run( dma_writer_fsm_ptr_t p_fsm );

int acamera_frame_fr_set_ready_interrupt( acamera_firmware_t *g_fw );
void dma_writer_update_address_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event );
void acamera_frame_buffer_update( dma_writer_fsm_const_ptr_t p_fsm );


struct _dma_writer_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    dma_writer_state_t state;
    fsm_irq_mask_t mask;
    uint32_t vflip;

    dma_type dma_reader_out;
    uint8_t context_created;
    int32_t dis_ds_base_offset;
    void *handle;
};

void dma_writer_fsm_clear( dma_writer_fsm_ptr_t p_fsm );
void dma_writer_fsm_init( void *fsm, fsm_init_param_t *init_param );
int dma_writer_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int dma_writer_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );
uint8_t dma_writer_fsm_process_event( dma_writer_fsm_ptr_t p_fsm, event_id_t event_id );

void dma_writer_fsm_process_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void dma_writer_request_interrupt( dma_writer_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __DMA_WRITER_FSM_H__ */
