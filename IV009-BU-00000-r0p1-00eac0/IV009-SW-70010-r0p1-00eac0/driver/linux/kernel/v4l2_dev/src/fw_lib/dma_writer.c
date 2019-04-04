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

#include "acamera_logger.h"
#include "system_interrupts.h"
#include "system_stdlib.h"
#include "dma_writer_api.h"
#include "dma_writer.h"
#include "acamera_firmware_config.h"
#include "acamera.h"
#include "acamera_fw.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_DMA_WRITER
#endif

extern uint32_t acamera_get_api_context( void );

#define TAG "DMA_WRITER"

//#define DMA_PRINTF(a) printf a
#define DMA_PRINTF( a ) (void)0


static void dma_writer_initialize_reg_ops( dma_pipe *pipe )
{
    dma_writer_reg_ops_t *primary_ops = &pipe->primary;
    dma_writer_reg_ops_t *secondary_ops = &pipe->secondary;

    primary_ops->format_read = pipe->api.p_acamera_isp_dma_writer_format_read;
    primary_ops->format_write = pipe->api.p_acamera_isp_dma_writer_format_write;
    primary_ops->bank0_base_write = pipe->api.p_acamera_isp_dma_writer_bank0_base_write;
    primary_ops->line_offset_write = pipe->api.p_acamera_isp_dma_writer_line_offset_write;
    primary_ops->write_on_write = pipe->api.p_acamera_isp_dma_writer_frame_write_on_write;
    primary_ops->active_width_write = pipe->api.p_acamera_isp_dma_writer_active_width_write;
    primary_ops->active_height_write = pipe->api.p_acamera_isp_dma_writer_active_height_write;
    primary_ops->active_width_read = pipe->api.p_acamera_isp_dma_writer_active_width_read;
    primary_ops->active_height_read = pipe->api.p_acamera_isp_dma_writer_active_height_read;

    secondary_ops->format_read = pipe->api.p_acamera_isp_dma_writer_format_read_uv;
    secondary_ops->format_write = pipe->api.p_acamera_isp_dma_writer_format_write_uv;
    secondary_ops->bank0_base_write = pipe->api.p_acamera_isp_dma_writer_bank0_base_write_uv;
    secondary_ops->line_offset_write = pipe->api.p_acamera_isp_dma_writer_line_offset_write_uv;
    secondary_ops->write_on_write = pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv;
    secondary_ops->active_width_write = pipe->api.p_acamera_isp_dma_writer_active_width_write_uv;
    secondary_ops->active_height_write = pipe->api.p_acamera_isp_dma_writer_active_height_write_uv;
    secondary_ops->active_width_read = pipe->api.p_acamera_isp_dma_writer_active_width_read_uv;
    secondary_ops->active_height_read = pipe->api.p_acamera_isp_dma_writer_active_height_read_uv;
}

#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
static int dma_writer_alloc_default_frame( dma_pipe *pipe )
{
    acamera_settings *settings = &pipe->settings.p_ctx->settings;
    uint32_t ctx_id = pipe->settings.p_ctx->context_id;
    tframe_t *default_frame;
    void *virt_addr;
    uint64_t dma_addr;
    int i;
    tframe_t *curr_frame = &pipe->settings.curr_frame;
    tframe_t *done_frame = &pipe->settings.done_frame;
    tframe_t *delay_frame = &pipe->settings.delay_frame;
    dma_writer_reg_ops_t *primary_ops = &pipe->primary;

    pipe->settings.default_index = 0;

    for ( i = 0; i < DMA_WRITER_DEFAULT_BUFFER_NO; i++ ) {
        default_frame = &pipe->settings.default_frame[i];
        system_memset( default_frame, 0, sizeof( *default_frame ) );

        default_frame->primary.type = primary_ops->format_read( pipe->settings.isp_base );
        default_frame->primary.status = dma_buf_empty;
        default_frame->primary.width = primary_ops->active_width_read( pipe->settings.isp_base );
        default_frame->primary.height = primary_ops->active_height_read( pipe->settings.isp_base );
        default_frame->primary.line_offset = acamera_line_offset( default_frame->primary.width,
                                                                  _get_pixel_width( default_frame->primary.type ) );
        default_frame->primary.size = default_frame->primary.height * default_frame->primary.line_offset;

        virt_addr = settings->callback_dma_alloc_coherent(
            ctx_id, default_frame->primary.size, &dma_addr );
        if ( !virt_addr ) {
            LOG( LOG_CRIT, "Pipe: %d unable to allocate default buffer", pipe->type );
            return -1;
        }

        default_frame->primary.address = (uint32_t)dma_addr;
        default_frame->primary.virt_addr = virt_addr;

        LOG( LOG_INFO, "Pipe: %d Default buffer size: %d dma_addr: %llx",
             pipe->type, default_frame->primary.size, default_frame->primary.address );

        /* for the default frames we don't use the uv plane */
        default_frame->secondary.status = dma_buf_purge;
    }

    *curr_frame = pipe->settings.default_frame[pipe->settings.default_index];
    *done_frame = pipe->settings.default_frame[pipe->settings.default_index];
    *delay_frame = pipe->settings.default_frame[pipe->settings.default_index];

    pipe->settings.default_index = ( pipe->settings.default_index + 1 ) % 2;

    return 0;
}

static void dma_writer_free_default_frame( dma_pipe *pipe )
{
    acamera_settings *settings = &pipe->settings.p_ctx->settings;
    uint32_t ctx_id = pipe->settings.p_ctx->context_id;
    tframe_t *default_frame;
    void *virt_addr;
    uint64_t dma_addr;
    uint64_t size;
    int i;

    for ( i = 0; i < DMA_WRITER_DEFAULT_BUFFER_NO; i++ ) {
        default_frame = &pipe->settings.default_frame[i];
        dma_addr = default_frame->primary.address;
        virt_addr = default_frame->primary.virt_addr;
        size = default_frame->primary.size;

        settings->callback_dma_free_coherent( ctx_id, size, virt_addr, dma_addr );

        system_memset( default_frame, 0, sizeof( *default_frame ) );
    }
}
#endif

static int dma_writer_stream_get_frame( dma_pipe *pipe, tframe_t *tframe )
{
    uint32_t ctx_id = pipe->settings.p_ctx->context_id;
    acamera_stream_type_t type = pipe->type == dma_fr ? ACAMERA_STREAM_FR : ACAMERA_STREAM_DS1;
    acamera_settings *settings = &pipe->settings.p_ctx->settings;
    aframe_t tmp_aframes[2];
    int rc;

    tmp_aframes[0] = tframe->primary;
    tmp_aframes[1] = tframe->secondary;

    rc = settings->callback_stream_get_frame( ctx_id, type, tmp_aframes, 2 );

    tframe->primary = tmp_aframes[0];
    tframe->secondary = tmp_aframes[1];

    return rc;
}

static int dma_writer_stream_put_frame( dma_pipe *pipe, tframe_t *tframe )
{
    uint32_t ctx_id = pipe->settings.p_ctx->context_id;
    acamera_stream_type_t type = pipe->type == dma_fr ? ACAMERA_STREAM_FR : ACAMERA_STREAM_DS1;
    acamera_settings *settings = &pipe->settings.p_ctx->settings;
    aframe_t tmp_aframes[2];
    int rc;

    tmp_aframes[0] = tframe->primary;
    tmp_aframes[1] = tframe->secondary;

    rc = settings->callback_stream_put_frame( ctx_id, type, tmp_aframes, 2 );

    tframe->primary = tmp_aframes[0];
    tframe->secondary = tmp_aframes[1];

    return rc;
}

#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
static int dma_writer_configure_frame_reader( dma_pipe *pipe, tframe_t *frame )
{
    aframe_t *aframe;

    if ( !( pipe->settings.active && pipe->settings.ctx_id == acamera_get_api_context() ) )
        return 0;

    aframe = &frame->primary;

    pipe->api.p_acamera_fpga_frame_reader_rbase_write( pipe->settings.isp_base, aframe->address );
    pipe->api.p_acamera_fpga_frame_reader_line_offset_write( pipe->settings.isp_base, aframe->line_offset );
    pipe->api.p_acamera_fpga_frame_reader_format_write( pipe->settings.isp_base, aframe->type );

    return 0;
}
#endif

static int dma_writer_configure_frame_writer( dma_pipe *pipe,
                                              aframe_t *aframe,
                                              dma_writer_reg_ops_t *reg_ops )
{
    uint32_t addr;
    uint32_t line_offset;

    if ( aframe->status != dma_buf_purge ) {
        /*
         * For now we don't change the settings, so we take them from the hardware
         * The reason is that they are configured through a different API
         */
        aframe->type = reg_ops->format_read( pipe->settings.isp_base );
        aframe->width = reg_ops->active_width_read( pipe->settings.isp_base );
        aframe->height = reg_ops->active_height_read( pipe->settings.isp_base );

        aframe->line_offset = acamera_line_offset( aframe->width, _get_pixel_width( aframe->type ) );
        aframe->size = aframe->line_offset * aframe->height;

        addr = aframe->address;
        line_offset = aframe->line_offset;

        if ( pipe->settings.vflip ) {
            addr += aframe->size - aframe->line_offset;
            line_offset = -aframe->line_offset;
        }

        reg_ops->format_write( pipe->settings.isp_base, aframe->type );
        reg_ops->active_width_write( pipe->settings.isp_base, aframe->width );
        reg_ops->active_height_write( pipe->settings.isp_base, aframe->height );
        reg_ops->line_offset_write( pipe->settings.isp_base, line_offset );
        reg_ops->bank0_base_write( pipe->settings.isp_base, addr );
        reg_ops->write_on_write( pipe->settings.isp_base, 1 );
    } else {
        reg_ops->write_on_write( pipe->settings.isp_base, 0 );
    }

    return 0;
}

static int dma_writer_configure_pipe( dma_pipe *pipe )
{
    struct _acamera_context_t *p_ctx = pipe->settings.p_ctx;
    metadata_t *meta = &pipe->settings.curr_metadata;
    tframe_t *curr_frame = &pipe->settings.curr_frame;
    tframe_t *done_frame = &pipe->settings.done_frame;
    tframe_t *delay_frame = &pipe->settings.delay_frame;
    dma_writer_reg_ops_t *primary_ops = &pipe->primary;
    dma_writer_reg_ops_t *secondary_ops = &pipe->secondary;
    int rc;

    if ( !p_ctx ) {
        LOG( LOG_ERR, "No context available." );
        return -1;
    }

    curr_frame->primary.frame_id = meta->frame_id;
    curr_frame->secondary.frame_id = meta->frame_id;

#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
    dma_writer_configure_frame_reader( pipe, done_frame );
#endif

    /* put back the done frame to the application (V4L2 for example) */
    if ( done_frame->primary.status == dma_buf_busy ||
         done_frame->secondary.status == dma_buf_busy )
        dma_writer_stream_put_frame( pipe, done_frame );

    /* done_frame is the last delayed frame */
    *done_frame = *delay_frame;

    /* delay_frame is the last current frame written in the software config */
    *delay_frame = *curr_frame;

    /* try to get a new buffer from application (V4l2 for example) */
    rc = dma_writer_stream_get_frame( pipe, curr_frame );
    if ( rc ) {
#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
        /* if there is no available buffer, use one of the default buffers */
        *curr_frame = pipe->settings.default_frame[pipe->settings.default_index];
        pipe->settings.default_index = ( pipe->settings.default_index + 1 ) % DMA_WRITER_DEFAULT_BUFFER_NO;
#else
        curr_frame->primary.status = dma_buf_purge;
        curr_frame->secondary.status = dma_buf_purge;
#endif
    } else {
        if ( curr_frame->primary.status == dma_buf_empty )
            curr_frame->primary.status = dma_buf_busy;
        if ( curr_frame->secondary.status == dma_buf_empty )
            curr_frame->secondary.status = dma_buf_busy;
    }

    /* write the current frame in the software config */
    dma_writer_configure_frame_writer( pipe, &curr_frame->primary, primary_ops );
    dma_writer_configure_frame_writer( pipe, &curr_frame->secondary, secondary_ops );

    return 0;
}

static dma_handle s_handle[FIRMWARE_CONTEXT_NUMBER];
static int32_t ctx_pos = 0;
dma_error dma_writer_create( void **handle )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {

        if ( ctx_pos < acamera_get_context_number() ) {
            *handle = &s_handle[ctx_pos];
            system_memset( (void *)*handle, 0, sizeof( dma_handle ) );
            ctx_pos++;
        } else {
            *handle = NULL;
            result = edma_fail;
            LOG( LOG_ERR, "dma context overshoot\n" );
        }
    } else {
        result = edma_fail;
    }
    return result;
}

void dma_writer_exit( void *handle )
{
#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
    dma_handle *p_dma = (dma_handle *)handle;

    dma_writer_free_default_frame( &p_dma->pipe[dma_fr] );
#if ISP_HAS_DS1
    dma_writer_free_default_frame( &p_dma->pipe[dma_ds1] );
#endif
#endif

    ctx_pos = 0;
}

dma_error dma_writer_update_state( dma_pipe *pipe )
{
    dma_error result = edma_ok;
    if ( pipe != NULL ) {
        LOG( LOG_INFO, "state update %dx%d\n", (int)pipe->settings.width, (int)pipe->settings.height );
        if ( pipe->settings.width == 0 || pipe->settings.height == 0 ) {
            result = edma_invalid_settings;
            pipe->settings.enabled = 0;
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, pipe->settings.enabled ); //disable pipe on invalid settings
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, pipe->settings.enabled );
            return result;
        }


        //set default settings here
        pipe->api.p_acamera_isp_dma_writer_max_bank_write( pipe->settings.isp_base, 0 );
        pipe->api.p_acamera_isp_dma_writer_active_width_write( pipe->settings.isp_base, pipe->settings.width );
        pipe->api.p_acamera_isp_dma_writer_active_height_write( pipe->settings.isp_base, pipe->settings.height );

        pipe->api.p_acamera_isp_dma_writer_max_bank_write_uv( pipe->settings.isp_base, 0 );
        pipe->api.p_acamera_isp_dma_writer_active_width_write_uv( pipe->settings.isp_base, pipe->settings.width );
        pipe->api.p_acamera_isp_dma_writer_active_height_write_uv( pipe->settings.isp_base, pipe->settings.height );
    } else {
        result = edma_fail;
    }
    return result;
}


dma_error dma_writer_get_settings( void *handle, dma_type type, dma_pipe_settings *settings )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        system_memcpy( (void *)settings, &p_dma->pipe[type].settings, sizeof( dma_pipe_settings ) );
    } else {
        result = edma_fail;
    }
    return result;
}


dma_error dma_writer_set_settings( void *handle, dma_type type, dma_pipe_settings *settings )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        system_memcpy( (void *)&p_dma->pipe[type].settings, settings, sizeof( dma_pipe_settings ) );
        result = dma_writer_update_state( &p_dma->pipe[type] );

    } else {
        LOG( LOG_ERR, "set settings handle null\n" );
        result = edma_fail;
    }
    return result;
}

dma_error dma_writer_get_ptr_settings( void *handle, dma_type type, dma_pipe_settings **settings )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        *settings = &p_dma->pipe[type].settings;
    } else {
        result = edma_fail;
    }
    return result;
}

void dma_writer_set_initialized( void *handle, dma_type type, uint8_t initialized )
{
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        p_dma->pipe[type].state.initialized = initialized;
    }
}

dma_error dma_writer_init( void *handle, dma_type type, dma_pipe_settings *settings, dma_api *api )
{
    dma_error result = edma_ok;
#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
    int rc;
#endif

    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        p_dma->pipe[type].type = type;

        system_memcpy( (void *)&p_dma->pipe[type].api, api, sizeof( dma_api ) );
        result = dma_writer_set_settings( handle, type, settings );

        dma_writer_initialize_reg_ops( &p_dma->pipe[type] );

#if CONFIG_DMA_WRITER_DEFAULT_BUFFER
        rc = dma_writer_alloc_default_frame( &p_dma->pipe[type] );
        if ( rc ) {
            LOG( LOG_ERR, "%s: Failed to alloc default frame type:%d", TAG, (int)type );
        }
#endif

        if ( result == edma_ok ) {
            LOG( LOG_INFO, "%s: initialized a dma writer output. %dx%d ", TAG, (int)settings->width, (int)settings->height );
        } else {
            LOG( LOG_ERR, "%s: Failed to initialize a dma writer type:%d output. %dx%d ", TAG, (int)type, (int)settings->width, (int)settings->height );
        }
    } else {
        result = edma_fail;
    }
    return result;
}

dma_error dma_writer_pipe_process_interrupt( dma_pipe *pipe, uint32_t irq_event )
{
    switch ( irq_event ) {
    case ACAMERA_IRQ_FRAME_WRITER_FR:
        if ( pipe->type == dma_fr ) {
            dma_writer_configure_pipe( pipe );
        }
        break;
    case ACAMERA_IRQ_FRAME_WRITER_DS:
        if ( pipe->type == dma_ds1 ) {
            dma_writer_configure_pipe( pipe );
        }
        break;
    }

    return 0;
}


dma_error dma_writer_process_interrupt( void *handle, uint32_t irq_event )
{
    dma_error result = edma_ok;
    uint32_t idx = 0;

    dma_handle *p_dma = (dma_handle *)handle;
    if ( !p_dma ) {
        LOG( LOG_ERR, "p_dma is NULL of event: %u", (unsigned int)irq_event );
        return edma_fail;
    }

    for ( idx = 0; idx < dma_max; idx++ ) {
        dma_writer_pipe_process_interrupt( &p_dma->pipe[idx], irq_event );
    }
    return result;
}


metadata_t *dma_writer_return_metadata( void *handle, dma_type type )
{
    if ( handle != NULL ) {

        dma_handle *p_dma = (dma_handle *)handle;

        metadata_t *ret = &( p_dma->pipe[type].settings.curr_metadata );
        return ret;
    }
    return 0;
}
