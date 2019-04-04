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

#ifndef _ISP_V4L2_STREAM_H_
#define _ISP_V4L2_STREAM_H_

#include <linux/videodev2.h>
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 4, 4, 0 ) )
#include <media/videobuf2-v4l2.h>
#endif

#include "acamera_firmware_api.h"

#include "isp-v4l2-common.h"

/* buffer for one video frame */
typedef struct _isp_v4l2_buffer {
/* vb or vvb (depending on kernel version) must be first */
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 4, 4, 0 ) )
    struct vb2_v4l2_buffer vvb;
#else
    struct vb2_buffer vb;
#endif

    struct list_head list;
} isp_v4l2_buffer_t;

/**
 * struct isp_v4l2_stream_common
 */
typedef struct _isp_v4l2_frame_sizes {
    /* resolution table for FR stream */
    struct v4l2_frmsize_discrete frmsize[MAX_SENSOR_PRESET_SIZE]; /* for now this is same since FR path doesn't have downscaler block */
    uint8_t frmsize_num;
} isp_v4l2_frame_sizes;

typedef struct _isp_v4l2_stream_common {
    isp_v4l2_sensor_info sensor_info;
    isp_v4l2_frame_sizes snapshot_sizes;
} isp_v4l2_stream_common;

/**
 * struct isp_v4l2_stream_t - All internal data for one instance of ISP
 */
typedef struct _isp_v4l2_stream_t {
    /* Control fields */
    uint32_t ctx_id;
    int stream_id;
    isp_v4l2_stream_type_t stream_type;
    int stream_started;
    uint32_t last_frame_id;

    /* Input stream */
    isp_v4l2_stream_common *stream_common;

    /* Stream format */
    struct v4l2_format cur_v4l2_fmt;

    /* Video buffer field*/
    struct list_head stream_buffer_list;
    struct list_head stream_buffer_list_busy;
    spinlock_t slock;

    /* Temporal fields for memcpy */
#if ISP_HAS_META_CB
    atomic_t running; //since metadata has no thread for syncing
#endif
    int fw_frame_seq_count;
} isp_v4l2_stream_t;


/* stream control interface */
int isp_v4l2_stream_init_static_resources( uint32_t ctx_id );
void isp_v4l2_stream_deinit_static_resources( void );
int isp_v4l2_stream_init( isp_v4l2_stream_t **ppstream, int stream_id, int ctx_num );
void isp_v4l2_stream_deinit( isp_v4l2_stream_t *pstream );
int isp_v4l2_stream_on( isp_v4l2_stream_t *pstream );
void isp_v4l2_stream_off( isp_v4l2_stream_t *pstream );

/* stream configuration interface */
int isp_v4l2_stream_enum_framesizes( isp_v4l2_stream_t *pstream, struct v4l2_frmsizeenum *fsize );
int isp_v4l2_stream_enum_format( isp_v4l2_stream_t *pstream, struct v4l2_fmtdesc *f );
int isp_v4l2_stream_try_format( isp_v4l2_stream_t *pstream, struct v4l2_format *f );
int isp_v4l2_stream_get_format( isp_v4l2_stream_t *pstream, struct v4l2_format *f );
int isp_v4l2_stream_set_format( isp_v4l2_stream_t *pstream, struct v4l2_format *f );

#endif
