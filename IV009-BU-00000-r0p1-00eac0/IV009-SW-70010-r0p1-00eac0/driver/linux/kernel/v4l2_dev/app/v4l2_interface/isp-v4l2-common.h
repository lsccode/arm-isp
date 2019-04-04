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

#ifndef _ISP_V4L2_COMMON_H_
#define _ISP_V4L2_COMMON_H_

#include "acamera_firmware_config.h"

/* Sensor data types */
#define MAX_SENSOR_PRESET_SIZE 10
#define MAX_SENSOR_FPS_SIZE 10
typedef struct _isp_v4l2_sensor_preset {
    uint32_t width;
    uint32_t height;
    uint32_t fps[MAX_SENSOR_FPS_SIZE];
    uint32_t idx[MAX_SENSOR_FPS_SIZE];
    uint8_t fps_num;
    uint8_t fps_cur;
    uint8_t exposures[MAX_SENSOR_FPS_SIZE];
} isp_v4l2_sensor_preset;

typedef struct _isp_v4l2_sensor_info {
    /* resolution preset */
    isp_v4l2_sensor_preset preset[MAX_SENSOR_PRESET_SIZE];
    uint8_t preset_num;
    uint8_t preset_cur;
} isp_v4l2_sensor_info;

#define V4L2_CAN_UPDATE_SENSOR 0
#define V4L2_RESTORE_FR_BASE0 1

/* custom v4l2 formats */
#define ISP_V4L2_PIX_FMT_META v4l2_fourcc( 'M', 'E', 'T', 'A' )        /* META */
#define ISP_V4L2_PIX_FMT_ARGB2101010 v4l2_fourcc( 'B', 'A', '3', '0' ) /* ARGB2101010 */
#define ISP_V4L2_PIX_FMT_NULL v4l2_fourcc( 'N', 'U', 'L', 'L' )        /* format NULL to disable */

/* custom v4l2 events */
#define V4L2_EVENT_ACAMERA_CLASS ( V4L2_EVENT_PRIVATE_START + 0xA * 1000 )
#define V4L2_EVENT_ACAMERA_FRAME_READY ( V4L2_EVENT_ACAMERA_CLASS + 0x1 )
#define V4L2_EVENT_ACAMERA_STREAM_OFF ( V4L2_EVENT_ACAMERA_CLASS + 0x2 )

/* custom v4l2 controls */
#define ISP_V4L2_CID_ISP_V4L2_CLASS ( 0x00f00000 | 1 )
#define ISP_V4L2_CID_BASE ( 0x00f00000 | 0xf000 )
#define ISP_V4L2_CID_TEST_PATTERN ( ISP_V4L2_CID_BASE + 0 )
#define ISP_V4L2_CID_TEST_PATTERN_TYPE ( ISP_V4L2_CID_BASE + 1 )
#define ISP_V4L2_CID_AF_REFOCUS ( ISP_V4L2_CID_BASE + 2 )
#define ISP_V4L2_CID_SENSOR_PRESET ( ISP_V4L2_CID_BASE + 3 )
#define ISP_V4L2_CID_AF_ROI ( ISP_V4L2_CID_BASE + 4 )
#define ISP_V4L2_CID_OUTPUT_FR_ON_OFF ( ISP_V4L2_CID_BASE + 5 )
#define ISP_V4L2_CID_OUTPUT_DS1_ON_OFF ( ISP_V4L2_CID_BASE + 6 )


/* type of stream */
typedef enum {
    V4L2_STREAM_TYPE_FR = 0,
#if ISP_HAS_META_CB
    V4L2_STREAM_TYPE_META,
#endif
#if ISP_HAS_DS1
    V4L2_STREAM_TYPE_DS1,
#endif
#if ISP_HAS_RAW_CB
    V4L2_STREAM_TYPE_RAW,
#endif
    V4L2_STREAM_TYPE_MAX
} isp_v4l2_stream_type_t;

#endif
