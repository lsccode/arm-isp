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

#if !defined( __AF_FSM_H__ )
#define __AF_FSM_H__



typedef struct _AF_fsm_t AF_fsm_t;
typedef struct _AF_fsm_t *AF_fsm_ptr_t;
typedef const struct _AF_fsm_t *AF_fsm_const_ptr_t;


enum _AF_status_t {
    AF_STATUS_FAST_SEARCH,
    AF_STATUS_TRACKING,
    AF_STATUS_CONVERGED,
    AF_STATUS_INVALID,
};

typedef enum _AF_status_t AF_status_t;

#define ISP_HAS_AF_FSM 1
#include "acamera_isp_config.h"
#include "acamera_lens_api.h"
#include "acamera_isp_core_nomem_settings.h"

#define ISP_HAS_AF_LMS_FSM 1
#define AF_ZONES_COUNT_MAX ( ISP_METERING_ZONES_MAX_V * ISP_METERING_ZONES_MAX_H )


#define AF_SPOT_IGNORE_NUM 1
#define AF_CALIBRATION_BOUNDARIES 1

#define AF_SPOT_STATUS_NOT_FINISHED 0
#define AF_SPOT_STATUS_FINISHED_VALID 1
#define AF_SPOT_STATUS_FINISHED_INVALID 2

typedef struct _af_lms_param_t {
    uint32_t pos_min_down;
    uint32_t pos_min;
    uint32_t pos_min_up;
    uint32_t pos_inf_down;
    uint32_t pos_inf;
    uint32_t pos_inf_up;
    uint32_t pos_macro_down;
    uint32_t pos_macro;
    uint32_t pos_macro_up;
    uint32_t pos_max_down;
    uint32_t pos_max;
    uint32_t pos_max_up;
    uint32_t fast_search_positions;
    uint32_t skip_frames_init;
    uint32_t skip_frames_move;
    uint32_t dynamic_range_th;
    uint32_t spot_tolerance;
    uint32_t exit_th;
    uint32_t caf_trigger_th;
    uint32_t caf_stable_th;
    uint32_t print_debug;
} af_lms_param_t;
typedef struct _af_spot_param_t {
    int32_t max_value;
    int32_t min_value;
    uint32_t max_position;
    int32_t before_max_value;
    uint32_t before_max_position;
    int32_t after_max_value;
    uint32_t after_max_position;
    int32_t previous_value;
    uint32_t optimal_position;
    uint8_t status;
    uint32_t reliable;
    uint32_t dynamic_range;
} af_spot_param_t;
typedef struct _af_fast_search_param_t {
    uint32_t step;
    uint32_t step_number;
    uint32_t position;
    uint32_t prev_position;
    uint32_t spot_zone_step_x;
    uint32_t spot_zone_step_y;
    uint32_t spot_zone_size_x;
    uint32_t spot_zone_size_y;
    af_spot_param_t spots[AF_SPOT_COUNT_Y][AF_SPOT_COUNT_X];
    uint32_t finished_valid_spot_count;
} af_fast_search_param_t;

typedef struct _af_track_position_param_t {
    uint8_t frames_in_tracking;
    uint8_t scene_is_changed;
    int32_t values[10];
    int values_inx;
} af_track_position_param_t;

void AF_init( AF_fsm_ptr_t p_fsm );
void AF_deinit( AF_fsm_ptr_t p_fsm );
void AF_request_irq( AF_fsm_ptr_t p_fsm );
void AF_init_fast_search( AF_fsm_ptr_t p_fsm );
void AF_process_fast_search_step( AF_fsm_ptr_t p_fsm );
void AF_init_track_position( AF_fsm_ptr_t p_fsm );
void AF_process_track_position_step( AF_fsm_ptr_t p_fsm );
#include "acamera_metering_stats_mem_config.h"

struct _AF_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint32_t zone_raw_statistic[AF_ZONES_COUNT_MAX][2];
    uint8_t zone_weight[AF_ZONES_COUNT_MAX];
    uint64_t zone_process_statistic[AF_ZONES_COUNT_MAX];
    uint32_t zone_process_reliablility[AF_ZONES_COUNT_MAX];
    uint32_t pos_min;
    uint32_t pos_inf;
    uint32_t pos_macro;
    uint32_t pos_max;
    uint32_t def_pos_min;
    uint32_t def_pos_inf;
    uint32_t def_pos_macro;
    uint32_t def_pos_max;
    uint32_t def_pos_min_up;
    uint32_t def_pos_inf_up;
    uint32_t def_pos_macro_up;
    uint32_t def_pos_max_up;
    uint32_t def_pos_min_down;
    uint32_t def_pos_inf_down;
    uint32_t def_pos_macro_down;
    uint32_t def_pos_max_down;
    uint8_t skip_frame;
    af_fast_search_param_t fs;
    af_track_position_param_t tp;
    uint8_t frame_num;
    uint8_t mode;
    uint32_t pos_manual;
    uint32_t roi;
    int32_t lens_driver_ok;
    uint32_t roi_api;
    uint32_t frame_number_from_start;
    uint32_t last_position;
    int32_t last_sharp;
    void *lens_ctx;
    lens_control_t lens_ctrl;
    AF_status_t af_status;
    lens_param_t lens_param;
#if USER_MODULE
    uint8_t skip_cur_frame;
#endif
};


void AF_fsm_clear( AF_fsm_ptr_t p_fsm );

void AF_fsm_init( void *fsm, fsm_init_param_t *init_param );
int AF_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int AF_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t AF_fsm_process_event( AF_fsm_ptr_t p_fsm, event_id_t event_id );
void AF_fsm_process_interrupt( AF_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void AF_request_interrupt( AF_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __AF_FSM_H__ */
