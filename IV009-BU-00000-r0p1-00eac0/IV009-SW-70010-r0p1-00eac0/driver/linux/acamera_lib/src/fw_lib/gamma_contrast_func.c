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

#include "acamera_fw.h"
#include "acamera_math.h"
#include "acamera_logger.h"
#include "system_stdlib.h"
#include "acamera_ihist_stats_mem_config.h"
#include "acamera_command_api.h"
#include "gamma_contrast_fsm.h"


#include "acamera_fr_gamma_rgb_mem_config.h"

#if ISP_HAS_DS1
#include "acamera_ds1_gamma_rgb_mem_config.h"
#endif


#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_GAMMA_CONTRAST
#endif

void gamma_contrast_read_histogram( gamma_contrast_fsm_ptr_t p_fsm )
{
    int i;
    uint32_t sum = 0;

    fsm_param_mon_alg_flow_t gamma_flow;

    gamma_flow.frame_id_tracking = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    p_fsm->cur_using_stats_frame_id = gamma_flow.frame_id_tracking;

    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; ++i ) {
        uint32_t v = acamera_ihist_stats_mem_array_data_read( p_fsm->cmn.isp_base, i );

        v <<= 8;

        p_fsm->fullhist[i] = v;
        sum += v;
    }
    p_fsm->fullhist_sum = sum;


    gamma_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    gamma_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_GAMMA_FLOW, &gamma_flow, sizeof( gamma_flow ) );
    LOG( LOG_INFO, "Gamma flow: INPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", gamma_flow.frame_id_tracking, gamma_flow.frame_id_current );
}

void gamma_contrast_process( gamma_contrast_fsm_ptr_t p_fsm )
{
    uint32_t i;

    //tuning params
    int32_t black_percentage = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[0];
    int32_t white_percentage = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[1];
    int32_t auto_black_min = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[2];
    int32_t auto_black_max = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[3];
    int32_t auto_white_prc_target = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[4];
    int32_t avg_coeff = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[5];
    int32_t enable_auto_level = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AUTO_LEVEL_CONTROL )[6];

    uint32_t auto_white_percentage = ISP_FULL_HISTOGRAM_SIZE * white_percentage / 100; // white_percentage% percentile
    uint32_t auto_black_percentage = ISP_FULL_HISTOGRAM_SIZE * black_percentage / 100; // black_percentage% percentile
    uint32_t shift_number = acamera_log2_fixed_to_fixed( ISP_FULL_HISTOGRAM_SIZE, 0, 0 );

    uint32_t auto_level_offset = 0;
    uint32_t auto_level_gain = 0;
    uint32_t apply_gain;

    if ( enable_auto_level ) {
        uint64_t pixel_count = 0;
        uint32_t thr = 0;
        uint32_t auto_white_index = 0;
        uint32_t auto_black_index = 0;
        uint32_t white_gain = 256;

        p_fsm->cumu_hist[0] = p_fsm->fullhist[0];
        for ( i = 1; i < ISP_FULL_HISTOGRAM_SIZE; i++ ) {
            p_fsm->cumu_hist[i] = p_fsm->cumu_hist[i - 1] + p_fsm->fullhist[i];
        }

        pixel_count = p_fsm->cumu_hist[ISP_FULL_HISTOGRAM_SIZE - 1];
        thr = ( auto_white_percentage * pixel_count ) >> shift_number;
        i = ISP_FULL_HISTOGRAM_SIZE - 1;
        while ( p_fsm->cumu_hist[i] >= thr && i > 1 ) {
            i--;
        }
        auto_white_index = i;
        auto_white_index = ( auto_white_index <= ISP_FULL_HISTOGRAM_SIZE / 2 ) ? ISP_FULL_HISTOGRAM_SIZE / 2 : auto_white_index;
        white_gain = ( ( ISP_FULL_HISTOGRAM_SIZE * auto_white_prc_target / 100 ) << 8 ) / auto_white_index; //U24.8

        int32_t max_gain_clipping;
        // hard coded to 99% of the histogram as we want to avoide clipping
        max_gain_clipping = ( ( ISP_FULL_HISTOGRAM_SIZE * 99 / 100 ) << 8 ) / auto_white_index; //U24.8

        // Calculate black I cut
        thr = ( auto_black_percentage * pixel_count ) >> shift_number;
        i = auto_white_index;
        while ( p_fsm->cumu_hist[i] >= thr && i > 1 ) {
            i--;
        }
        auto_black_index = i;

        int32_t contrast = auto_white_index / ( auto_black_index ? auto_black_index : 1 );

        auto_black_index = auto_black_index < auto_black_min ? auto_black_min : auto_black_index;
        auto_black_index = auto_black_index > auto_black_max ? auto_black_max : auto_black_index;
        auto_level_offset = auto_black_index;

        // This will prevent applyign gain in LDR scenes
        uint32_t max_gain_contrast = ISP_FULL_HISTOGRAM_SIZE * 256 / ( ( ISP_FULL_HISTOGRAM_SIZE - auto_level_offset ) ? ( ISP_FULL_HISTOGRAM_SIZE - auto_level_offset ) : 1 );
        int32_t m, cx1 = 30, cx2 = 50, cy1 = 256, cy2 = 0, alpha = 0;
        m = ( ( cy1 - cy2 ) * 256 ) / ( cx1 - cx2 );
        alpha = ( ( m * ( contrast - cx1 ) >> 8 ) + cy1 ); //U32.0
        alpha = alpha < 0 ? 0 : alpha;
        alpha = alpha > 256 ? 256 : alpha;
        max_gain_clipping = ( ( alpha * max_gain_contrast ) + ( ( 256 - alpha ) * max_gain_clipping ) ) >> 8;

        auto_level_gain = ISP_FULL_HISTOGRAM_SIZE * 256 / ( ( ISP_FULL_HISTOGRAM_SIZE - auto_level_offset ) ? ( ISP_FULL_HISTOGRAM_SIZE - auto_level_offset ) : 1 );
        auto_level_gain = auto_level_gain < white_gain ? white_gain : auto_level_gain;               //max(auto_level_gain,white_gain)
        auto_level_gain = auto_level_gain > max_gain_clipping ? max_gain_clipping : auto_level_gain; //min(auto_level_gain,max_gain_clipping)
        auto_level_gain = auto_level_gain > 4095 ? 4095 : auto_level_gain;                           //u4.8 register
        //LOG( LOG_INFO, " white_gain %d max_gain_clipping %d auto_level_gain %d black %d auto_white_index %d contrast %d al %d m %d %d", white_gain, max_gain_clipping, auto_level_gain, auto_level_offset, auto_white_index, contrast, alpha, m, max_gain_contrast );

        // LOG( LOG_INFO, "auto_white_index: %u, auto_black_index: %u, auto_level_offset: %u, auto_level_gain: %u, apply_gain: %u.",
        //       auto_white_index, auto_black_index, auto_level_offset, auto_level_gain, apply_gain );
    } else {
        auto_level_gain = 256;
        auto_level_offset = 0;
    }

    //time average gain
    apply_gain = p_fsm->gain_target = auto_level_gain;
    if ( avg_coeff > 1 ) {
        ( (gamma_contrast_fsm_ptr_t)p_fsm )->gain_avg += p_fsm->gain_target - p_fsm->gain_avg / avg_coeff; // division by zero is checked
        apply_gain = ( uint16_t )( p_fsm->gain_avg / avg_coeff );                                          // division by zero is checked
    }

    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_auto_level == 0 ) {
        acamera_isp_fr_gamma_rgb_gain_r_write( p_fsm->cmn.isp_base, apply_gain );
        acamera_isp_fr_gamma_rgb_gain_g_write( p_fsm->cmn.isp_base, apply_gain );
        acamera_isp_fr_gamma_rgb_gain_b_write( p_fsm->cmn.isp_base, apply_gain );

        acamera_isp_fr_gamma_rgb_offset_r_write( p_fsm->cmn.isp_base, auto_level_offset );
        acamera_isp_fr_gamma_rgb_offset_g_write( p_fsm->cmn.isp_base, auto_level_offset );
        acamera_isp_fr_gamma_rgb_offset_b_write( p_fsm->cmn.isp_base, auto_level_offset );


#if ISP_HAS_DS1
        acamera_isp_ds1_gamma_rgb_gain_r_write( p_fsm->cmn.isp_base, apply_gain );
        acamera_isp_ds1_gamma_rgb_gain_g_write( p_fsm->cmn.isp_base, apply_gain );
        acamera_isp_ds1_gamma_rgb_gain_b_write( p_fsm->cmn.isp_base, apply_gain );

        acamera_isp_ds1_gamma_rgb_offset_r_write( p_fsm->cmn.isp_base, auto_level_offset );
        acamera_isp_ds1_gamma_rgb_offset_g_write( p_fsm->cmn.isp_base, auto_level_offset );
        acamera_isp_ds1_gamma_rgb_offset_b_write( p_fsm->cmn.isp_base, auto_level_offset );
#endif
        p_fsm->gamma_gain = apply_gain;
        p_fsm->gamma_offset = auto_level_offset;
    }

    // skip when frame_id is 0.
    if ( p_fsm->cur_using_stats_frame_id ) {
        fsm_param_mon_alg_flow_t gamma_flow;

        gamma_flow.frame_id_tracking = p_fsm->cur_using_stats_frame_id;
        gamma_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        gamma_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_GAMMA_FLOW, &gamma_flow, sizeof( gamma_flow ) );
        LOG( LOG_INFO, "Gamma flow: OUTPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", gamma_flow.frame_id_tracking, gamma_flow.frame_id_current );

        gamma_flow.frame_id_tracking = p_fsm->cur_using_stats_frame_id;
        gamma_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        gamma_flow.flow_state = MON_ALG_FLOW_STATE_APPLIED;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_GAMMA_FLOW, &gamma_flow, sizeof( gamma_flow ) );
        LOG( LOG_INFO, "Gamma flow: APPLIED: frame_id_tracking: %d, cur frame_id: %u.", gamma_flow.frame_id_tracking, gamma_flow.frame_id_current );
    }
}

void gamma_contrast_init( gamma_contrast_fsm_ptr_t p_fsm )
{
    LOG( LOG_INFO, "Initialize gamma contrast FSM" );

    gamma_contrast_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_ANTIFOG_HIST ) );
}


void gamma_contrast_fsm_process_interrupt( gamma_contrast_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    if ( irq_event == ACAMERA_IRQ_ANTIFOG_HIST ) {
        gamma_contrast_read_histogram( (gamma_contrast_fsm_ptr_t)p_fsm );
        fsm_raise_event( p_fsm, event_id_gamma_contrast_stats_ready );
    }
    return;
}
