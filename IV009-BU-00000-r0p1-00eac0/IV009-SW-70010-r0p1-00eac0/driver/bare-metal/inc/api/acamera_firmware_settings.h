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

#ifndef __ACAMERA_FIRMWARE_SETTNGS_H__
#define __ACAMERA_FIRMWARE_SETTNGS_H__


#include "acamera_sensor_api.h"
#include "acamera_lens_api.h"

// formats which are supported on output of ISP
// they can be used to set a desired output format
// in acamera_settings structure
#define DMA_FORMAT_DISABLE      0
#define DMA_FORMAT_RGB32        1
#define DMA_FORMAT_A2R10G10B10  2
#define DMA_FORMAT_RGB565       3
#define DMA_FORMAT_RGB24        4
#define DMA_FORMAT_GEN32        5
#define DMA_FORMAT_RAW16        6
#define DMA_FORMAT_RAW12        7
#define DMA_FORMAT_AYUV         8
#define DMA_FORMAT_Y410         9
#define DMA_FORMAT_YUY2         10
#define DMA_FORMAT_UYVY         11
#define DMA_FORMAT_Y210         12
#define DMA_FORMAT_NV12_Y       13
#define DMA_FORMAT_NV12_UV      (1<<6|13)
#define DMA_FORMAT_NV12_VU      (2<<6|13)
#define DMA_FORMAT_YV12_Y       14
#define DMA_FORMAT_YV12_U       (1<<6|14)
#define DMA_FORMAT_YV12_V       (2<<6|14)


static inline uint32_t _get_pixel_width( uint32_t format )
{
  uint32_t result = 4 ;
  switch(format)
  {

  case DMA_FORMAT_RGB565:
  case DMA_FORMAT_RAW16:
  case DMA_FORMAT_YUY2:
  case DMA_FORMAT_UYVY:
  case DMA_FORMAT_RAW12:
    result = 2;
    break;
  case DMA_FORMAT_NV12_Y:
  case DMA_FORMAT_NV12_UV:
  case DMA_FORMAT_NV12_VU:
  case DMA_FORMAT_YV12_Y:
  case DMA_FORMAT_YV12_U:
  case DMA_FORMAT_YV12_V:
    result = 1;
    break;
  }
  return result ;
}


// calibration table.
typedef struct _ACameraCalibrations {
    LookupTable* calibrations[ CALIBRATION_TOTAL_SIZE ] ;
} ACameraCalibrations ;



typedef struct _aframe_t {
    uint32_t type ;        // frame type.
    uint32_t width ;       // frame width
    uint32_t height ;      // frame height
    uint32_t address ;     // start of memory block
    uint32_t line_offset ; // line offset for the frame
    uint32_t size ; // total size of the memory in bytes
    uint32_t status ;
    void *virt_addr ;      // virt address accessed by cpu
    uint32_t frame_id ;
} aframe_t ;

typedef struct _tframe_t {
    aframe_t primary;        //primary frames
    aframe_t secondary ;        //secondary frames
} tframe_t ;


// this structure is used by firmware to return the information
// about an output frame
typedef struct _metadata_t {
    uint8_t format;
    uint16_t width;
    uint16_t height;
    uint32_t frame_number;
    uint16_t line_size;
    uint32_t exposure;
    uint32_t int_time;
    uint32_t int_time_medium;
    uint32_t int_time_long;
    uint32_t again;
    uint32_t dgain;
    uint32_t addr;
    uint32_t isp_dgain;
    int8_t dis_offset_x;
    int8_t dis_offset_y;
    uint32_t frame_id;
} metadata_t ;


typedef struct _acamera_settings {
    void (*sensor_init)( void** ctx, sensor_control_t* ctrl) ;         // must be initialized to provide sensor initialization entry. Must be provided.
    void (*sensor_deinit)( void *ctx ) ;         // must be initialized to provide sensor initialization entry. Must be provided.
    int32_t (*lens_init)( void** ctx, lens_control_t* ctrl) ;          // initialize lens driver for AF. May be NULL. Must return 0 on success and -1 on fail.
    void (*lens_deinit)( void *ctx ) ;         // initialize lens driver for AF. May be NULL. Must return 0 on success and -1 on fail.
    uint32_t (*get_calibrations)( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations * ) ;  // must be initialized to provide calibrations. Must be provided.
    uintptr_t isp_base ;                                                 // isp base offset (not absolute memory address ). Should be started from start of isp memory. All ISP r/w accesses inside the firmware will use this value as the start_offset.
    uint32_t hw_isp_addr ;                                               // hardware isp register configuration address.
    void *(*callback_dma_alloc_coherent)( uint32_t ctx_id, uint64_t size, uint64_t *dma_addr );
    void (*callback_dma_free_coherent)( uint32_t ctx_id, uint64_t size, void *virt_addr, uint64_t dma_addr );
    int (*callback_stream_get_frame)( uint32_t ctx_id, acamera_stream_type_t type, aframe_t *aframes, uint64_t num_planes );
    int (*callback_stream_put_frame)( uint32_t ctx_id, acamera_stream_type_t type, aframe_t *aframes, uint64_t num_planes );
} acamera_settings ;

#endif
