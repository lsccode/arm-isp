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

#ifndef __DMA_WRITER_API__
#define __DMA_WRITER_API__

#include "acamera_types.h"
#include "acamera_firmware_config.h"
#include "acamera.h"

#define CONFIG_DMA_WRITER_DEFAULT_BUFFER        (ISP_HAS_FPGA_WRAPPER && ISP_CONTROLS_DMA_READER)
#define DMA_WRITER_DEFAULT_BUFFER_NO            3

/**
 *   return codes
 */
typedef enum dma_error {
    edma_ok = 0,
    edma_fail,
    edma_invalid_pipe,
    edma_wrong_parameters,
    edma_invalid_api,
    edma_invalid_settings
} dma_error;


/**
 *   pipe api
 *   all functions must be initialized properly
 */
typedef struct dma_api {
    uint8_t ( *p_acamera_isp_dma_writer_format_read )( uintptr_t );
    void ( *p_acamera_isp_dma_writer_format_write )( uintptr_t, uint8_t );
    void ( *p_acamera_isp_dma_writer_bank0_base_write )( uintptr_t, uint32_t );
#if ISP_HAS_FPGA_WRAPPER
    void ( *p_acamera_fpga_frame_reader_rbase_write )( uintptr_t, uint32_t );
    void ( *p_acamera_fpga_frame_reader_line_offset_write )( uintptr_t, uint32_t );
    void (*p_acamera_fpga_frame_reader_format_write) (uintptr_t base, uint8_t data);
    void (*p_acamera_fpga_frame_reader_rbase_load_write) (uintptr_t base, uint8_t data);
#endif
    void ( *p_acamera_isp_dma_writer_line_offset_write )( uintptr_t, uint32_t );
    void ( *p_acamera_isp_dma_writer_frame_write_on_write )( uintptr_t, uint8_t );
    void ( *p_acamera_isp_dma_writer_active_width_write )( uintptr_t, uint16_t );
    void ( *p_acamera_isp_dma_writer_active_height_write )( uintptr_t, uint16_t );

    uint16_t ( *p_acamera_isp_dma_writer_active_width_read )( uintptr_t );
    uint16_t ( *p_acamera_isp_dma_writer_active_height_read )( uintptr_t );

    void ( *p_acamera_isp_dma_writer_max_bank_write )( uintptr_t, uint8_t );

    uint8_t ( *p_acamera_isp_dma_writer_format_read_uv )( uintptr_t );
    void ( *p_acamera_isp_dma_writer_format_write_uv )( uintptr_t, uint8_t );
    void ( *p_acamera_isp_dma_writer_bank0_base_write_uv )( uintptr_t, uint32_t );
    void ( *p_acamera_isp_dma_writer_max_bank_write_uv )( uintptr_t, uint8_t );
    void ( *p_acamera_isp_dma_writer_line_offset_write_uv )( uintptr_t, uint32_t );
    void ( *p_acamera_isp_dma_writer_frame_write_on_write_uv )( uintptr_t, uint8_t );
    void ( *p_acamera_isp_dma_writer_active_width_write_uv )( uintptr_t, uint16_t );
    void ( *p_acamera_isp_dma_writer_active_height_write_uv )( uintptr_t, uint16_t );
    uint16_t ( *p_acamera_isp_dma_writer_active_width_read_uv )( uintptr_t );
    uint16_t ( *p_acamera_isp_dma_writer_active_height_read_uv )( uintptr_t );
#if ISP_HAS_FPGA_WRAPPER
    void ( *p_acamera_fpga_frame_reader_rbase_write_uv )( uintptr_t, uint32_t );
    void ( *p_acamera_fpga_frame_reader_line_offset_write_uv )( uintptr_t, uint32_t );
    void (*p_acamera_fpga_frame_reader_format_write_uv) (uintptr_t base, uint8_t data);
    void (*p_acamera_fpga_frame_reader_rbase_load_write_uv) (uintptr_t base, uint8_t data);
#endif

} dma_api;


/**
 *   pipe settings
 */
typedef struct dma_pipe_settings {
    uint32_t width;  // dma output width
    uint32_t height; // dma output height

#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
    tframe_t default_frame[DMA_WRITER_DEFAULT_BUFFER_NO]; // default double buffering
    uint8_t default_index; // marks either ping or pong default buffer
#endif
    tframe_t curr_frame; // current frame written in the software configuration
    tframe_t delay_frame; // delayed frame due to pipeline delay
    tframe_t done_frame; // frame done means filled by the dma writer

    metadata_t curr_metadata;

    uint32_t vflip; // vertical flip

    int32_t read_offset;  // offset for frame reader
    int32_t write_offset; // offset for frame writer
    uint32_t active;      // frame reader gets data from active pipe
    uint32_t enabled;     // enable or disable any pipe activity

    uint32_t banks_number; // number of used banks
    uint32_t clear_memory; // clear memory befor writing
    uint32_t clear_color;  // clear color
    uint32_t last_address; // last address which was used to write a frame
    uintptr_t isp_base;
    uint32_t ctx_id;
    uint8_t pause;
    struct _acamera_context_t *p_ctx;
} dma_pipe_settings;


/**
 *   Create a dma writer instance
 *
 *   This routine creates a dma writer instance.
 *   The output pointer to dma writer structure will be saved in *handle
 *
 *   @param handle - pointer to the instance
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_create( void **handle );
void dma_writer_exit( void *handle );

/**
 *   Check if the pipe correctly initialized
 *
 *
 *   @param handle - pointer to the instance
 *
 *   @return edma_ok - initialized properly
 *           edma_fail - fail
 */
dma_error dma_writer_pipe_initialized( void **handle, dma_type type );


/**
 *   Initialize a pipe
 *
 *   This routine will initialize a pipe with specified settings and given api
 *   Be sure that all api functions have been initialized properly
 *
 *   @param handle - pointer to the instance
 *   @param settings - pointer to the settings structure.
 *   @param api - pointer to the api structure.
 *   @param type - a type of a dma output
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_init( void *handle, dma_type type, dma_pipe_settings *settings, dma_api *api );


/**
 *   Return current settings
 *
 *   The routine will copy all settings to your memory
 *   for specified pipe. It's better to use this function when you're
 *   planning to call set_settings after it.
 *
 *   @param handle - pointer to the instance
 *   @param settings - pointer to the settings structure.
 *   @param type - a type of a dma output
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_get_settings( void *handle, dma_type type, dma_pipe_settings *settings );


/**
 *   Return current settings
 *
 *   The routine will return a pointer on a settings structure
 *   for a given pipe. It can be used if you need just a fast access
 *   to the settings without modifying it
 *
 *   @param handle - pointer to the instance
 *   @param settings - pointer to the settings structure.
 *   @param type - a type of a dma output
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_get_ptr_settings( void *handle, dma_type type, dma_pipe_settings **settings );

/**
 *   Apply new settings for specified pipe
 *
 *   This routine should be used to change the settings of a pipe.
 *
 *   @param handle - pointer to the instance
 *   @param settings - pointer to the settings structure.
 *   @param type - a type of a dma output
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_set_settings( void *handle, dma_type type, dma_pipe_settings *settings );


/**
 *   Reset a pipe to the initial state
 *
 *   The routine will reset a specified pipe to its initial state
 *
 *   @param handle - pointer to the instance
 *   @param type - a type of a dma output
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_reset( void *handle, dma_type type );


/**
 *   Process an interrupt
 *
 *   The routine will handle an interrupt to update
 *   a state of all pipes
 *
 *   @param handle - pointer to the instance
 *   @param irq_event - input interrupt number
 *
 *   @return edma_ok - success
 *           edma_fail - fail
 */
dma_error dma_writer_process_interrupt( void *handle, uint32_t irq_event );

metadata_t *dma_writer_return_metadata( void *handle, dma_type type );
void dma_writer_set_initialized( void *handle, dma_type type, uint8_t initialized );

#endif /* __DMA_WRITER_API__ */
