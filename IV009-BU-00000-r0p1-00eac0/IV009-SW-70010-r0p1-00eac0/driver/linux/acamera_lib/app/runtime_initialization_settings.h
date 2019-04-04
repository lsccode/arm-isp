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

#include "acamera_types.h"
#include "acamera_firmware_config.h"
#include "acamera_firmware_api.h"
#include "acamera_sensor_api.h"
#include "acamera_lens_api.h"



extern void sensor_init_dummy( void** ctx, sensor_control_t*) ;
extern void sensor_deinit_dummy( void *ctx ) ;
extern uint32_t user2kernel_get_calibrations( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

#if FIRMWARE_CONTEXT_NUMBER == 2


extern void sensor_init_dummy( void** ctx, sensor_control_t*) ;
extern void sensor_deinit_dummy( void *ctx ) ;
extern uint32_t user2kernel_get_calibrations( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

#endif // FIRMWARE_CONTEXT_NUMBER == 2

extern int32_t lens_init( void** ctx, lens_control_t* ctrl ) ;
extern void lens_deinit( void * ctx) ;



extern void *callback_dma_alloc_coherent( uint32_t ctx_id, uint64_t size, uint64_t *dma_addr ) ;
extern void callback_dma_free_coherent( uint32_t ctx_id, uint64_t size, void *virt_addr, uint64_t dma_addr ) ;

extern int callback_stream_get_frame( uint32_t ctx_id, acamera_stream_type_t type, aframe_t *aframes, uint64_t num_planes ) ;
extern int callback_stream_put_frame( uint32_t ctx_id, acamera_stream_type_t type, aframe_t *aframes, uint64_t num_planes ) ;

static acamera_settings settings[ FIRMWARE_CONTEXT_NUMBER ] = {    {
        .sensor_init = sensor_init_dummy,
        .sensor_deinit = sensor_deinit_dummy,
        .get_calibrations = user2kernel_get_calibrations,
        .lens_init = lens_init,
        .lens_deinit = lens_deinit,
        .isp_base = 0x0,
        .hw_isp_addr = 0x0,
    },

#if FIRMWARE_CONTEXT_NUMBER == 2
    {
        .sensor_init = sensor_init_dummy,
        .sensor_deinit = sensor_deinit_dummy,
        .get_calibrations = user2kernel_get_calibrations,
        .lens_init = lens_init,
        .lens_deinit = lens_deinit,
        .isp_base = 0x0,
        .hw_isp_addr = 0x0,
    },

#endif // FIRMWARE_CONTEXT_NUMBER == 2
} ;
