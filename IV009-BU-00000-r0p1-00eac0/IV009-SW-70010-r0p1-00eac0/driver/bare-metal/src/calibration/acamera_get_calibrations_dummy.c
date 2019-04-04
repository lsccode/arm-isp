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

#include "acamera_command_api.h"
#include "acamera_sensor_api.h"
#include "acamera_firmware_settings.h"
#include "acamera_logger.h"

extern uint32_t get_calibrations_static_linear_dummy( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_dummy( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_dummy( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_dummy( ACameraCalibrations *c );

uint32_t get_calibrations_dummy( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;

    if ( !sensor_arg ) {
        LOG( LOG_ERR, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_dummy( c ) + get_calibrations_static_linear_dummy( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_dummy( c ) + get_calibrations_static_fs_lin_dummy( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_dummy( c ) + get_calibrations_static_linear_dummy( c ) );
        break;
    }

    return ret;
}
