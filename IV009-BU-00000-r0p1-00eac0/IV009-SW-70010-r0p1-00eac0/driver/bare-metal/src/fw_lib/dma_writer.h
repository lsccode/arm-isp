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
