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

/*
 * ACamera PCI Express UIO driver
 *
 */
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/interrupt.h>
#include <linux/of_reserved_mem.h>
#include <asm/io.h>
#include <asm/signal.h>

#include "acamera_firmware_config.h"
#include "acamera_logger.h"

#define LOG_CONTEXT "[ ACamera ]"

#define ISP_V4L2_MODULE_NAME "isp-v4l2"

#include "v4l2_interface/isp-v4l2.h"

extern void system_interrupts_set_irq( int irq_num, int flags );

//map and unmap fpga memory
extern int32_t init_hw_io( resource_size_t addr, resource_size_t size );
extern int32_t close_hw_io( void );

static struct v4l2_device v4l2_dev;
static int initialized = 0;

int otp_enable = 0;

static const struct of_device_id isp_dt_match[] = {
    {.compatible = "arm,isp"},
    {}};

MODULE_DEVICE_TABLE( of, isp_dt_match );


#if V4L2_SOC_SUBDEV_ENABLE
extern uint32_t fw_calibration_update( void );
#include "soc_iq.h"

static int v4l2devs_running = 0;

struct acamera_v4l2_subdev_t {

    struct v4l2_subdev *soc_subdevs[V4L2_SOC_SUBDEV_NUMBER];
    struct v4l2_async_subdev soc_async_sd[V4L2_SOC_SUBDEV_NUMBER];
    struct v4l2_async_subdev *soc_async_sd_ptr[V4L2_SOC_SUBDEV_NUMBER];
    int subdev_counter;
    struct v4l2_async_notifier notifier;
    uint32_t hw_isp_addr;
};

static struct acamera_v4l2_subdev_t g_subdevs;


void *acamera_camera_v4l2_get_subdev_by_name( const char *name )
{
    int idx = 0;
    void *result = NULL;
    LOG( LOG_INFO, "Requested a pointer to the subdev with a name %s", name );
    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        if ( g_subdevs.soc_subdevs[idx] && strcmp( g_subdevs.soc_subdevs[idx]->name, name ) == 0 ) {
            result = g_subdevs.soc_subdevs[idx];
            break;
        }
    }
    LOG( LOG_INFO, "Return subdev pointer 0x%x", result );
    return result;
}

int acamera_camera_v4l2_get_index_by_name( const char *name )
{
    int idx = 0;
    LOG( LOG_INFO, "Requested a index pointer with a name %s", name );
    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        if ( g_subdevs.soc_subdevs[idx] && strcmp( g_subdevs.soc_subdevs[idx]->name, name ) == 0 ) {
            break;
        }
    }
    LOG( LOG_INFO, "Return index pointer 0x%x", idx );
    return idx;
}

static int acamera_camera_async_bound( struct v4l2_async_notifier *notifier,
                                       struct v4l2_subdev *sd,
                                       struct v4l2_async_subdev *asd )
{
    int rc = 0;
    LOG( LOG_INFO, "bound called with sd 0x%x, asd 0x%x, sd->dev 0x%x, name %s", sd, asd, sd->dev, sd->name );
    int idx = 0;
    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        if ( g_subdevs.soc_subdevs[idx] == 0 ) {
            break;
        }
    }

    if ( idx < V4L2_SOC_SUBDEV_NUMBER ) {
        g_subdevs.soc_subdevs[idx] = sd;
        g_subdevs.subdev_counter++;

        if ( strcmp( g_subdevs.soc_subdevs[idx]->name, V4L2_SOC_IQ_NAME ) == 0 && v4l2devs_running == 1 ) { //update calibration
            fw_calibration_update();
        }

    } else {
        rc = -1;
        LOG( LOG_ERR, "Inserted more subdevices than expected. Driver is configured to support %d subdevs only", V4L2_SOC_SUBDEV_NUMBER );
    }


    return rc;
}


static void acamera_camera_async_unbind( struct v4l2_async_notifier *notifier,
                                         struct v4l2_subdev *sd,
                                         struct v4l2_async_subdev *asd )
{
    LOG( LOG_INFO, "unbind called for subdevice sd 0x%x, asd 0x%x, sd->dev 0x%x, name %s", sd, asd, sd->dev, sd->name );

    int idx = acamera_camera_v4l2_get_index_by_name( sd->name );

    if ( strcmp( g_subdevs.soc_subdevs[idx]->name, V4L2_SOC_IQ_NAME ) != 0 ) { //any other subdevs need to stop firmware
        if ( v4l2devs_running == 1 ) {
            LOG( LOG_INFO, "stopping V4L2 firmware" );
            isp_v4l2_destroy_instance();
            initialized = 0;
            v4l2devs_running = 0;
        }
    }

    if ( idx < V4L2_SOC_SUBDEV_NUMBER ) {
        g_subdevs.soc_subdevs[idx] = 0;
        g_subdevs.subdev_counter--;
    }
}


static int acamera_camera_async_complete( struct v4l2_async_notifier *notifier )
{
    int rc = 0;

    LOG( LOG_NOTICE, "[KeyMsg] async complete called." );

    if ( v4l2devs_running == 0 ) {
        LOG( LOG_INFO, "starting V4L2 firmware" );
        rc = v4l2_device_register_subdev_nodes( &v4l2_dev );

        if ( rc == 0 ) {
            rc = isp_v4l2_create_instance( &v4l2_dev, g_subdevs.hw_isp_addr );

            if ( rc == 0 ) {
                initialized = 1;
            } else {
                LOG( LOG_ERR, "Failed to register ISP v4l2 driver." );
                initialized = 0;
                rc = -1;
            }
        } else {
            rc = -1;
            LOG( LOG_ERR, "Failed to create subdevice nodes under /dev/v4l-subdevX" );
        }
        v4l2devs_running = 1;
    }

    LOG( LOG_NOTICE, "[KeyMsg] async complete done, rc: %d.", rc );
    return rc;
}

#endif


static int32_t isp_platform_probe( struct platform_device *pdev )
{
    int32_t rc = 0;
    struct resource *isp_res = NULL;

    // Initialize irq
    isp_res = platform_get_resource_byname( pdev,
                                            IORESOURCE_IRQ, "ISP" );

    if ( isp_res ) {
        LOG( LOG_INFO, "Juno isp irq = %d, flags = 0x%x !\n", (int)isp_res->start, (int)isp_res->flags );
        system_interrupts_set_irq( isp_res->start, isp_res->flags );
    } else {
        LOG( LOG_ERR, "Error, no isp_irq found from DT\n" );
        return -1;
    }

    rc = of_reserved_mem_device_init( &pdev->dev );
    if ( rc ) {
        LOG( LOG_ERR, "Error, Could not get reserved memory\n" );
    }

    isp_res = platform_get_resource( pdev,
                                     IORESOURCE_MEM, 0 );
    if ( isp_res ) {
        LOG( LOG_INFO, "Juno isp address = 0x%x, end = 0x%x !\n", (int)isp_res->start, (int)isp_res->end );
        if ( init_hw_io( isp_res->start, ( isp_res->end - isp_res->start ) + 1 ) != 0 ) {
            LOG( LOG_ERR, "Error on mapping memory! \n" );
        }
    } else {
        LOG( LOG_ERR, "Error, no IORESOURCE_MEM DT!\n" );
        return -1;
    }

    static atomic_t drv_instance = ATOMIC_INIT( 0 );
    v4l2_device_set_name( &v4l2_dev, ISP_V4L2_MODULE_NAME, &drv_instance );
    rc = v4l2_device_register( &pdev->dev, &v4l2_dev );
    if ( rc == 0 ) {
        LOG( LOG_INFO, "register v4l2 driver. result %d.", rc );
    } else {
        LOG( LOG_ERR, "failed to register v4l2 device. rc = %d", rc );
        goto free_res;
    }

#if V4L2_SOC_SUBDEV_ENABLE
    int idx;

    LOG( LOG_INFO, "--------------------------------" );
    LOG( LOG_INFO, "Register %d subdevices", V4L2_SOC_SUBDEV_NUMBER );
    LOG( LOG_INFO, "--------------------------------" );

    g_subdevs.subdev_counter = 0;

    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        g_subdevs.soc_async_sd[idx].match_type = V4L2_ASYNC_MATCH_CUSTOM;
        g_subdevs.soc_async_sd[idx].match.custom.match = NULL;

        g_subdevs.soc_async_sd_ptr[idx] = &g_subdevs.soc_async_sd[idx];
        g_subdevs.soc_subdevs[idx] = 0;
    }

    g_subdevs.hw_isp_addr = (uint32_t)isp_res->start;

    g_subdevs.notifier.bound = acamera_camera_async_bound;
    g_subdevs.notifier.complete = acamera_camera_async_complete;
    g_subdevs.notifier.unbind = acamera_camera_async_unbind;

    g_subdevs.notifier.subdevs = (struct v4l2_async_subdev **)&g_subdevs.soc_async_sd_ptr;
    g_subdevs.notifier.num_subdevs = V4L2_SOC_SUBDEV_NUMBER;

    rc = v4l2_async_notifier_register( &v4l2_dev, &g_subdevs.notifier );

    LOG( LOG_INFO, "Init finished. async register notifier result %d. Waiting for subdevices", rc );
#else
    // no subdevice is used
    rc = isp_v4l2_create_instance( &v4l2_dev, (uint32_t)isp_res->start );
    if ( rc < 0 )
        goto free_res;

    if ( rc == 0 ) {
        initialized = 1;
    } else {
        LOG( LOG_ERR, "Failed to register ISP v4l2 driver." );
        return -1;
    }

#endif

free_res:

    return rc;
}

static int isp_platform_remove( struct platform_device *pdev )
{
    LOG( LOG_NOTICE, "ISP Driver removed\n" );
    of_reserved_mem_device_release( &pdev->dev );

    return 0;
}

static struct platform_driver isp_platform_driver = {
    .driver = {
        .name = "arm,isp",
        .owner = THIS_MODULE,
        .of_match_table = isp_dt_match,
    },
    .remove = isp_platform_remove,
};

static int __init fw_module_init( void )
{
    int32_t rc = 0;

    LOG( LOG_NOTICE, "[KeyMsg] ISP main dev init." );

    rc = platform_driver_probe( &isp_platform_driver,
                                isp_platform_probe );

    LOG( LOG_NOTICE, "[KeyMsg] ISP main dev init done, rc: %d.", rc );

    return rc;
}

static void __exit fw_module_exit( void )
{
    LOG( LOG_NOTICE, "[KeyMsg] ISP main dev exit." );

    if ( initialized == 1 ) {
        isp_v4l2_destroy_instance();
        initialized = 0;
    }

#if V4L2_SOC_SUBDEV_ENABLE
    v4l2_async_notifier_unregister( &g_subdevs.notifier );
#endif
    v4l2_device_unregister( &v4l2_dev );
    close_hw_io();
    platform_driver_unregister( &isp_platform_driver );

    LOG( LOG_NOTICE, "[KeyMsg] ISP main dev exit done." );
}

module_init( fw_module_init );
module_exit( fw_module_exit );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "ARM IVG AC" );
