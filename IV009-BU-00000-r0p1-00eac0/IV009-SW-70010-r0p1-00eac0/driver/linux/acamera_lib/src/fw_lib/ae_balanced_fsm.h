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

#if !defined( __AE_FSM_H__ )
#define __AE_FSM_H__


#include <acamera_firmware_config.h>

typedef struct _AE_fsm_t AE_fsm_t;
typedef struct _AE_fsm_t *AE_fsm_ptr_t;
typedef const struct _AE_fsm_t *AE_fsm_const_ptr_t;

#define ISP_HAS_AE_FSM 1

enum { AE_CLIPPING_ANTIFLICKER_N = 5 };

typedef struct _ae_balanced_param_t {
    uint32_t pi_coeff;
    uint32_t target_point;
    uint32_t tail_weight;
    uint32_t long_clip;
    uint32_t er_avg_coeff;
    uint32_t hi_target_prc;
    uint32_t hi_target_p;
    uint32_t enable_iridix_gdg;
    uint32_t AE_tol;
} ae_balanced_param_t;
void ae_initialize( AE_fsm_ptr_t p_fsm );
void ae_calculate_target( AE_fsm_ptr_t p_fsm );
void ae_calculate_exposure( AE_fsm_ptr_t p_fsm );
void ae_roi_update( AE_fsm_ptr_t p_fsm );
void ae_exposure_correction( AE_fsm_ptr_t p_fsm, int32_t corr );

struct _AE_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint32_t cur_using_stats_frame_id;

    int32_t error_log2;
    int32_t ae_hist_mean;
    int32_t exposure_log2;
    int64_t integrator;
    uint32_t exposure_ratio;
    uint32_t exposure_ratio_avg;
    uint32_t fullhist[ISP_FULL_HISTOGRAM_SIZE];
    uint32_t fullhist_sum;
    uint32_t ae_roi_api;
    uint32_t roi;
    uint8_t save_hist_api;
#if FW_ZONE_AE
    uint8_t smart_zone_enable;
    uint16_t hist4[ACAMERA_ISP_METERING_AEXP_NODES_USED_HORIZ_DEFAULT * ACAMERA_ISP_METERING_AEXP_NODES_USED_VERT_DEFAULT];
    uint8_t zone_weight[ACAMERA_ISP_METERING_AEXP_NODES_USED_HORIZ_DEFAULT * ACAMERA_ISP_METERING_AEXP_NODES_USED_VERT_DEFAULT];
    uint8_t x1;
    uint8_t y1;
    uint8_t x2;
    uint8_t y2;
#endif

    uint64_t WSNR_cumsum[ISP_FULL_HISTOGRAM_SIZE];
    int32_t targets_history[AE_CLIPPING_ANTIFLICKER_N];
    uint32_t next_hist_idx;
    int32_t max_target;
};


void AE_fsm_clear( AE_fsm_ptr_t p_fsm );

void AE_fsm_init( void *fsm, fsm_init_param_t *init_param );
int AE_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int AE_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t AE_fsm_process_event( AE_fsm_ptr_t p_fsm, event_id_t event_id );

void AE_fsm_process_interrupt( AE_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void AE_request_interrupt( AE_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __AE_FSM_H__ */
