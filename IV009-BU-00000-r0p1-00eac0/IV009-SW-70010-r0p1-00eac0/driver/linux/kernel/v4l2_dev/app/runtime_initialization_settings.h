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
#include "acamera_firmware_config.h"
#include "acamera_firmware_api.h"
#include "acamera_sensor_api.h"
#include "acamera_lens_api.h"



extern void sensor_init_v4l2( void** ctx, sensor_control_t*) ;
extern void sensor_deinit_v4l2( void *ctx ) ;
extern uint32_t get_calibrations_v4l2( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

#if FIRMWARE_CONTEXT_NUMBER == 2


extern void sensor_init_v4l2( void** ctx, sensor_control_t*) ;
extern void sensor_deinit_v4l2( void *ctx ) ;
extern uint32_t get_calibrations_v4l2( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

#endif // FIRMWARE_CONTEXT_NUMBER == 2

extern int32_t lens_init( void** ctx, lens_control_t* ctrl ) ;
extern void lens_deinit( void * ctx) ;


extern void callback_meta( uint32_t ctx_num,  const void *fw_metadata) ;

extern void *callback_dma_alloc_coherent( uint32_t ctx_id, uint64_t size, uint64_t *dma_addr ) ;
extern void callback_dma_free_coherent( uint32_t ctx_id, uint64_t size, void *virt_addr, uint64_t dma_addr ) ;

extern int callback_stream_get_frame( uint32_t ctx_id, acamera_stream_type_t type, aframe_t *aframes, uint64_t num_planes ) ;
extern int callback_stream_put_frame( uint32_t ctx_id, acamera_stream_type_t type, aframe_t *aframes, uint64_t num_planes ) ;

static acamera_settings settings[ FIRMWARE_CONTEXT_NUMBER ] = {    {
        .sensor_init = sensor_init_v4l2,
        .sensor_deinit = sensor_deinit_v4l2,
        .get_calibrations = get_calibrations_v4l2,
        .lens_init = lens_init,
        .lens_deinit = lens_deinit,
        .isp_base = 0x0,
        .hw_isp_addr = 0x0,
        .callback_meta = callback_meta,
        .callback_dma_alloc_coherent = callback_dma_alloc_coherent,
        .callback_dma_free_coherent = callback_dma_free_coherent,
        .callback_stream_get_frame = callback_stream_get_frame,
        .callback_stream_put_frame = callback_stream_put_frame,
    },

#if FIRMWARE_CONTEXT_NUMBER == 2
    {
        .sensor_init = sensor_init_v4l2,
        .sensor_deinit = sensor_deinit_v4l2,
        .get_calibrations = get_calibrations_v4l2,
        .lens_init = lens_init,
        .lens_deinit = lens_deinit,
        .isp_base = 0x0,
        .hw_isp_addr = 0x0,
        .callback_meta = callback_meta,
        .callback_dma_alloc_coherent = callback_dma_alloc_coherent,
        .callback_dma_free_coherent = callback_dma_free_coherent,
        .callback_stream_get_frame = callback_stream_get_frame,
        .callback_stream_put_frame = callback_stream_put_frame,
    },

#endif // FIRMWARE_CONTEXT_NUMBER == 2
} ;
