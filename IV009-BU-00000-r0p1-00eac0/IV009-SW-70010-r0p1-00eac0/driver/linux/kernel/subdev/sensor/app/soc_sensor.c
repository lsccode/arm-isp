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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>
#include "acamera_logger.h"
#include "acamera_sensor_api.h"
#include "soc_sensor.h"
#include "acamera_firmware_config.h"
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "runtime_initialization_settings.h"

#define ARGS_TO_PTR( arg ) ( (struct soc_sensor_ioctl_args *)arg )


void ( *SOC_SENSOR_SENSOR_ENTRY_ARR[FIRMWARE_CONTEXT_NUMBER] )( void **ctx, sensor_control_t *ctrl ) = SENSOR_INIT_SUBDEV_FUNCTIONS;
void ( *SOC_SENSOR_SENSOR_RESET_ARR[FIRMWARE_CONTEXT_NUMBER] )( void *ctx ) = SENSOR_DEINIT_SUBDEV_FUNCTIONS;

static struct v4l2_subdev soc_sensor;

typedef struct _subdev_camera_ctx {
    void *camera_context;
    sensor_control_t camera_control;
} subdev_camera_ctx;

static subdev_camera_ctx s_ctx[FIRMWARE_CONTEXT_NUMBER];


static int camera_log_status( struct v4l2_subdev *sd )
{
    LOG( LOG_INFO, "log status called" );
    return 0;
}


static int camera_init( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;

    if ( val < FIRMWARE_CONTEXT_NUMBER && SOC_SENSOR_SENSOR_ENTRY_ARR[val] ) {

        s_ctx[val].camera_context = NULL;

        ( SOC_SENSOR_SENSOR_ENTRY_ARR[val] )( &( s_ctx[val].camera_context ), &( s_ctx[val].camera_control ) );

        if ( s_ctx[val].camera_context == NULL ) {
            LOG( LOG_ERR, "Failed to process camera_init for ctx:%d. Sensor is not initialized yet. camera_init must be called before", val );
            rc = -1;
            return rc;
        }

        LOG( LOG_INFO, "Sensor has been initialized for ctx:%d\n", val );
    } else {
        rc = -1;
        LOG( LOG_ERR, "Failed to process camera init for ctx:%d", val );
    }
    return rc;
}

static int camera_reset( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;
    if ( val < FIRMWARE_CONTEXT_NUMBER && SOC_SENSOR_SENSOR_RESET_ARR[val] ) {
        ( SOC_SENSOR_SENSOR_RESET_ARR[val] )( s_ctx[val].camera_context );
    } else {
        rc = -1;
        LOG( LOG_ERR, "Failed to process camera reset for ctx:%d", val );
    }

    LOG( LOG_INFO, "Sensor has been reset for ctx:%d\n", val );
    return rc;
}


static long camera_ioctl( struct v4l2_subdev *sd, unsigned int cmd, void *arg )
{
    long rc = 0;

    if ( ARGS_TO_PTR( arg )->ctx_num >= FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_ERR, "Failed to process camera_ioctl for ctx:%d\n", ARGS_TO_PTR( arg )->ctx_num );
        return -1;
    }

    subdev_camera_ctx *ctx = &s_ctx[ARGS_TO_PTR( arg )->ctx_num];

    if ( ctx->camera_context == NULL ) {
        LOG( LOG_ERR, "Failed to process camera_ioctl. Sensor is not initialized yet. camera_init must be called before" );
        rc = -1;
        return rc;
    }


    const sensor_param_t *params = ctx->camera_control.get_parameters( ctx->camera_context );

    switch ( cmd ) {
    case SOC_SENSOR_STREAMING_ON:
        ctx->camera_control.start_streaming( ctx->camera_context );
        break;
    case SOC_SENSOR_STREAMING_OFF:
        ctx->camera_control.stop_streaming( ctx->camera_context );
        break;
    case SOC_SENSOR_SET_PRESET:
        ctx->camera_control.set_mode( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_ALLOC_AGAIN:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.alloc_analog_gain( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_ALLOC_DGAIN:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.alloc_digital_gain( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_ALLOC_IT:
        ctx->camera_control.alloc_integration_time( ctx->camera_context, &ARGS_TO_PTR( arg )->args.integration_time.it_short, &ARGS_TO_PTR( arg )->args.integration_time.it_medium, &ARGS_TO_PTR( arg )->args.integration_time.it_long );
        break;
    case SOC_SENSOR_UPDATE_EXP:
        ctx->camera_control.sensor_update( ctx->camera_context );
        break;
    case SOC_SENSOR_READ_REG:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.read_sensor_register( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_WRITE_REG:
        ctx->camera_control.write_sensor_register( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in, ARGS_TO_PTR( arg )->args.general.val_in2 );
        break;
    case SOC_SENSOR_GET_PRESET_NUM:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->modes_num;
        break;
    case SOC_SENSOR_GET_PRESET_CUR:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->mode;
        break;
    case SOC_SENSOR_GET_PRESET_WIDTH: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].resolution.width;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_HEIGHT: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].resolution.height;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_FPS: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].fps;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_SENSOR_BITS: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].bits;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_EXP: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].exposures;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_MODE: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].wdr_mode;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_EXP_NUMBER:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->sensor_exp_number;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_max;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_MIN:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_min;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_LONG_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_long_max;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_LIMIT:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_limit;
        break;
    case SOC_SENSOR_GET_ANALOG_GAIN_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->again_log2_max;
        break;
    case SOC_SENSOR_GET_DIGITAL_GAIN_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->dgain_log2_max;
        break;
    case SOC_SENSOR_GET_UPDATE_LATENCY:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_apply_delay;
        break;
    case SOC_SENSOR_GET_LINES_PER_SECOND:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->lines_per_second;
        break;
    case SOC_SENSOR_GET_FPS: {
        int mode = ARGS_TO_PTR( arg )->args.general.val_in;
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->modes_table[mode].fps;
    } break;
    case SOC_SENSOR_GET_ACTIVE_HEIGHT: {
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->active.height;
    } break;
    case SOC_SENSOR_GET_ACTIVE_WIDTH: {
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->active.width;
    } break;
    default:
        LOG( LOG_WARNING, "Unknown soc sensor ioctl cmd %d", cmd );
        rc = -1;
        break;
    };

    return rc;
}


static const struct v4l2_subdev_core_ops core_ops = {
    .log_status = camera_log_status,
    .init = camera_init,
    .reset = camera_reset,
    .ioctl = camera_ioctl,
};


static const struct v4l2_subdev_ops camera_ops = {
    .core = &core_ops,
};


static int32_t soc_sensor_probe( struct platform_device *pdev )
{
    int32_t rc = 0;

    v4l2_subdev_init( &soc_sensor, &camera_ops );

    soc_sensor.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

    snprintf( soc_sensor.name, V4L2_SUBDEV_NAME_SIZE, "%s", V4L2_SOC_SENSOR_NAME );

    soc_sensor.dev = &pdev->dev;
    rc = v4l2_async_register_subdev( &soc_sensor );

    LOG( LOG_NOTICE, "register v4l2 sensor device. result %d, sd 0x%x sd->dev 0x%x", rc, &soc_sensor, soc_sensor.dev );

    return rc;
}

static int soc_sensor_remove( struct platform_device *pdev )
{
    v4l2_async_unregister_subdev( &soc_sensor );

    return 0;
}

static struct platform_device *soc_sensor_dev;

static struct platform_driver soc_sensor_driver = {
    .probe = soc_sensor_probe,
    .remove = soc_sensor_remove,
    .driver = {
        .name = "soc_sensor_v4l2",
        .owner = THIS_MODULE,
    },
};

int __init acamera_camera_sensor_init( void )
{
    int rc = 0;
    LOG( LOG_INFO, "[KeyMsg] Sensor subdevice init" );

    soc_sensor_dev = platform_device_register_simple(
        "soc_sensor_v4l2", -1, NULL, 0 );
    rc = platform_driver_register( &soc_sensor_driver );

    LOG( LOG_INFO, "[KeyMsg] Sensor subdevice init done, rc: %d", rc );
    return rc;
}


void __exit acamera_camera_sensor_exit( void )
{
    LOG( LOG_INFO, "[KeyMsg] Sensor subdevice exit" );
    platform_driver_unregister( &soc_sensor_driver );
    platform_device_unregister( soc_sensor_dev );
    LOG( LOG_INFO, "[KeyMsg] Sensor subdevice exit done" );
}


module_init( acamera_camera_sensor_init );
module_exit( acamera_camera_sensor_exit );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "ARM IVG AC" );
