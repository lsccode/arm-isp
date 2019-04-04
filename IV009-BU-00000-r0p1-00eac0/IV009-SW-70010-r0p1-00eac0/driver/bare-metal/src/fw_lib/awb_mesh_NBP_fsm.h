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

#if !defined( __AWB_FSM_H__ )
#define __AWB_FSM_H__

#include "acamera_isp_core_nomem_settings.h"


typedef struct _AWB_fsm_t AWB_fsm_t;
typedef struct _AWB_fsm_t *AWB_fsm_ptr_t;
typedef const struct _AWB_fsm_t *AWB_fsm_const_ptr_t;


#define ISP_HAS_AWB_FSM 1
#define MAX_AWB_ZONES ( ISP_METERING_ZONES_MAX_V * ISP_METERING_ZONES_MAX_H )
#define AWB_LIGHT_SOURCE_UNKNOWN 0
#define AWB_LIGHT_SOURCE_A 0x01
#define AWB_LIGHT_SOURCE_D40 0x02
#define AWB_LIGHT_SOURCE_D50 0x03
#define AWB_LIGHT_SOURCE_A_TEMPERATURE 2850
#define AWB_LIGHT_SOURCE_D40_TEMPERATURE 4000
#define AWB_LIGHT_SOURCE_D50_TEMPERATURE 5000
#define AWB_DLS_LIGHT_SOURCE_A_D40_BORDER ( AWB_LIGHT_SOURCE_A_TEMPERATURE + AWB_LIGHT_SOURCE_D40_TEMPERATURE ) / 2
#define AWB_DLS_LIGHT_SOURCE_D40_D50_BORDER ( AWB_LIGHT_SOURCE_D40_TEMPERATURE + AWB_LIGHT_SOURCE_D50_TEMPERATURE ) / 2
#define AWB_DLS_SWITCH_LIGHT_SOURCE_DETECT_FRAMES_QUANTITY 15
#define AWB_DLS_SWITCH_LIGHT_SOURCE_CHANGE_FRAMES_QUANTITY 35
#define D50_DEFAULT 256

typedef struct _awb_zone_t {
    uint16_t rg;
    uint16_t bg;
    uint32_t sum;
} awb_zone_t;

void awb_init( AWB_fsm_ptr_t p_fsm );
void awb_coeffs_write( AWB_fsm_const_ptr_t p_fsm );
void awb_set_identity( AWB_fsm_ptr_t p_fsm );
void awb_read_statistics( AWB_fsm_ptr_t p_fsm );
void awb_calc_avg_weighted_gr_gb( AWB_fsm_ptr_t p_fsm );
void awb_detect_light_source( AWB_fsm_ptr_t p_fsm );
void awb_process_light_source( AWB_fsm_ptr_t p_fsm );
void awb_mesh_scene_detect_algo( AWB_fsm_ptr_t p_fsm );
void awb_update( AWB_fsm_ptr_t p_fsm );
void awb_normalise( AWB_fsm_ptr_t p_fsm );
void awb_roi_update( AWB_fsm_ptr_t p_fsm );


#include "acamera_metering_stats_mem_config.h"
#include "acamera_isp_core_nomem_settings.h"

struct _AWB_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint16_t curr_AWB_ZONES;
    uint8_t awb_enabled;
    uint32_t sum;
    uint16_t rg_coef;
    uint16_t bg_coef;
    uint8_t p_high;
    uint8_t p_low;
    int32_t high_temp;
    int32_t low_temp;
    int32_t sfact;
    int16_t g_avgShift;
    uint32_t avg_GR;
    uint32_t avg_GB;
    uint32_t stable_avg_RG;
    uint32_t stable_avg_BG;
    uint32_t valid_threshold;
    uint8_t gPrintCnt;
    uint16_t color_wb_matrix[9];
    awb_zone_t awb_zones[MAX_AWB_ZONES];
    int32_t weight_[MAX_AWB_ZONES];
    uint16_t rg_valid[MAX_AWB_ZONES];
    uint16_t bg_valid[MAX_AWB_ZONES];
    uint8_t mvalid[MAX_AWB_ZONES];
    uint8_t cwfzones[MAX_AWB_ZONES];
    uint8_t npcHigh[MAX_AWB_ZONES];
    uint8_t npcLow[MAX_AWB_ZONES];
    uint8_t sky_zones[MAX_AWB_ZONES];
    int32_t wb_log2[4];
    int32_t temperature_detected;
    uint8_t light_source_detected;
    uint8_t light_source_candidate;
    uint8_t detect_light_source_frames_count;
    uint32_t mode;
    int32_t max_temp;
    int32_t min_temp;
    uint16_t max_temp_rg;
    uint16_t max_temp_bg;
    uint16_t min_temp_rg;
    uint16_t min_temp_bg;
    uint32_t roi;

    int32_t awb_warming_A[3];
    int32_t awb_warming_D75[3];
    int32_t awb_warming_D50[3];
    int32_t awb_warming[3];
    uint32_t switch_light_source_detect_frames_quantity;
    uint32_t switch_light_source_change_frames_quantity;
    uint8_t is_ready;

    uint32_t cur_using_stats_frame_id;
    uint32_t cur_result_gain_frame_id;
};


void AWB_fsm_clear( AWB_fsm_ptr_t p_fsm );

void AWB_fsm_init( void *fsm, fsm_init_param_t *init_param );
int AWB_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int AWB_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t AWB_fsm_process_event( AWB_fsm_ptr_t p_fsm, event_id_t event_id );

void AWB_fsm_process_interrupt( AWB_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void AWB_request_interrupt( AWB_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __AWB_FSM_H__ */
