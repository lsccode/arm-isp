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

#ifndef __DMA_WRITER__
#define __DMA_WRITER__

#include "acamera_types.h"
#include "dma_writer_api.h"


typedef struct _dma_pipe_state {
    uint8_t initialized;
} dma_pipe_state;

typedef struct dma_writer_reg_ops {
    uint8_t ( *format_read )( uintptr_t );
    void ( *format_write )( uintptr_t, uint8_t );
    void ( *bank0_base_write )( uintptr_t, uint32_t );
    void ( *line_offset_write )( uintptr_t, uint32_t );
    void ( *write_on_write )( uintptr_t, uint8_t );
    void ( *active_width_write )( uintptr_t, uint16_t );
    void ( *active_height_write )( uintptr_t, uint16_t );
    uint16_t ( *active_width_read )( uintptr_t );
    uint16_t ( *active_height_read )( uintptr_t );
} dma_writer_reg_ops_t;

typedef struct _dma_pipe {
    dma_pipe_settings settings;
    dma_pipe_state state;
    dma_api api;
    dma_type type;
    dma_writer_reg_ops_t primary;
    dma_writer_reg_ops_t secondary;
} dma_pipe;


typedef struct _dma_handle {
    dma_pipe pipe[dma_max];
} dma_handle;


/**
 *   Get default settings for dis processor.
 *
 *   To be sure that all settings are correct you have to call this function before changing any parameters.
 *   It sets all settings in their default value. After that it can be changed by caller.
 *
 *   @param settings - pointer to the settings structure.
 *
 *   @return 0 - success
 *           1 - fail to set settings.
 */
dma_error dma_writer_update_state( dma_pipe *pipe );


#endif
