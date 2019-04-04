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

#include "acamera_firmware_api.h"
#include "acamera_fw.h"
#include "acamera_math.h"

#include "acamera_aexp_hist_stats_mem_config.h"

#include "acamera_math.h"

#include "ae_balanced_fsm.h"
#include "cmos_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AE_BALANCED
#endif

static const uint8_t AE_FXPOINT_QBITS = 6;
static const int32_t AE_CLIPPING_HISTORY_INIT_VALUE = 0; // Empty history

static uint8_t linear_interpolation_u8( int32_t x0, uint8_t y0, int32_t x1, uint8_t y1, int32_t x )
{
    if ( x1 != x0 )
        return y0 + ( y1 - y0 ) * ( x - x0 ) / ( x1 - x0 ); // division by zero is checked
    LOG( LOG_ERR, "AVOIDED DIVISION BY ZERO" );
    return y0;
}

static uint16_t get_exposure_correction( AE_fsm_ptr_t p_fsm )
{
    int32_t exp = acamera_math_exp2( p_fsm->exposure_log2, LOG2_GAIN_SHIFT, 6 );
    uint16_t correction = 0;
    //const uint8_t* lut = _GET_UCHAR_PTR(ACAMERA_FSM2CTX_PTR(p_fsm), _GET_HDR_TABLE_INDEX(CALIBRATION_AE_CORRECTION_START, ACAMERA_GET_WDR_MODE(p_fsm)) ) ;
    const uint8_t *lut = _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CORRECTION );

    if ( exp <= _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION )[0] )
        correction = lut[0];
    else if ( exp >= _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION ) - 1] )
        correction = lut[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION ) - 1];
    else {
        int i;
        for ( i = 1; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION ); i++ ) {
            if ( exp < _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION )[i] ) {
                correction = linear_interpolation_u8( _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION )[i - 1], lut[i - 1], _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_EXPOSURE_CORRECTION )[i], lut[i], exp );
                break;
            }
        }
    }
    return correction;
}
void ae_roi_update( AE_fsm_ptr_t p_fsm )
{
    uint16_t horz_zones = acamera_isp_metering_hist_aexp_nodes_used_horiz_read( p_fsm->cmn.isp_base );
    uint16_t vert_zones = acamera_isp_metering_hist_aexp_nodes_used_vert_read( p_fsm->cmn.isp_base );
    uint16_t x, y;

    uint16_t *ptr_ae_zone_whgh_h = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_HOR );
    uint16_t *ptr_ae_zone_whgh_v = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_VER );

    uint8_t x_start = ( uint8_t )( ( ( ( p_fsm->roi >> 24 ) & 0xFF ) * horz_zones + 128 ) >> 8 );
    uint8_t x_end = ( uint8_t )( ( ( ( p_fsm->roi >> 8 ) & 0xFF ) * horz_zones + 128 ) >> 8 );
    uint8_t y_start = ( uint8_t )( ( ( ( p_fsm->roi >> 16 ) & 0xFF ) * vert_zones + 128 ) >> 8 );
    uint8_t y_end = ( uint8_t )( ( ( ( p_fsm->roi >> 0 ) & 0xFF ) * vert_zones + 128 ) >> 8 );

    uint8_t zone_size_x = x_end - x_start;
    uint8_t zone_size_y = y_end - y_start;
    uint32_t middle_x = zone_size_x * 256 / 2;
    uint32_t middle_y = zone_size_y * 256 / 2;
    uint16_t scale_x = 0;
    uint16_t scale_y = 0;
    uint32_t ae_zone_wght_hor_len = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_HOR );
    uint32_t ae_zone_wght_ver_len = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_VER );

    if ( ae_zone_wght_hor_len ) {
        scale_x = ( horz_zones - 1 ) / ae_zone_wght_hor_len + 1;
    } else {
        LOG( LOG_ERR, "ae_zone_wght_hor_len is zero" );
        return;
    }
    if ( ae_zone_wght_ver_len ) {
        scale_y = ( vert_zones - 1 ) / ae_zone_wght_ver_len + 1;
    } else {
        LOG( LOG_ERR, "ae_zone_wght_ver_len is zero" );
        return;
    }

    uint16_t gaus_center_x = ( _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_HOR ) * 256 / 2 ) * scale_x;
    uint16_t gaus_center_y = ( _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_VER ) * 256 / 2 ) * scale_y;

    for ( y = 0; y < vert_zones; y++ ) {
        uint8_t ae_coeff = 0;
        for ( x = 0; x < horz_zones; x++ ) {
            if ( y >= y_start && y <= y_end &&
                 x >= x_start && x <= x_end ) {

                uint8_t index_y = ( y - y_start );
                uint8_t index_x = ( x - x_start );
                int32_t distance_x = ( index_x * 256 + 128 ) - middle_x;
                int32_t distance_y = ( index_y * 256 + 128 ) - middle_y;
                uint32_t coeff_x;
                uint32_t coeff_y;

                if ( ( x == x_end && x_start != x_end ) ||
                     ( y == y_end && y_start != y_end ) ) {
                    ae_coeff = 0;
                } else {
                    coeff_x = ( gaus_center_x + distance_x ) / 256;
                    if ( distance_x > 0 && ( distance_x & 0x80 ) )
                        coeff_x--;
                    coeff_y = ( gaus_center_y + distance_y ) / 256;
                    if ( distance_y > 0 && ( distance_y & 0x80 ) )
                        coeff_y--;

                    coeff_x = ptr_ae_zone_whgh_h[coeff_x / scale_x];
                    coeff_y = ptr_ae_zone_whgh_v[coeff_y / scale_y];

                    ae_coeff = ( coeff_x * coeff_y ) >> 4;
                    if ( ae_coeff > 1 )
                        ae_coeff--;
                }
            } else {
                ae_coeff = 0;
            }
            acamera_isp_metering_hist_aexp_zones_weight_write( p_fsm->cmn.isp_base, ISP_METERING_ZONES_MAX_H * y + x, ae_coeff );
        }
    }
}

void ae_initialize( AE_fsm_ptr_t p_fsm )
{
#if FW_ZONE_AE
    acamera_isp_metering_hist_thresh_0_1_write( p_fsm->cmn.isp_base, 0 );
    acamera_isp_metering_hist_thresh_1_2_write( p_fsm->cmn.isp_base, 0 );
    acamera_isp_metering_hist_thresh_3_4_write( p_fsm->cmn.isp_base, 0 );
    acamera_isp_metering_hist_thresh_4_5_write( p_fsm->cmn.isp_base, 224 );
    p_fsm->zone_weight[0] = 15;
#else
    {
        int i, j;
        for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ )
            for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ )
                acamera_isp_metering_hist_aexp_zones_weight_write( p_fsm->cmn.isp_base, ISP_METERING_ZONES_MAX_H * i + j, 15 );
    }
#endif

    {
        uint32_t i = 0;
        for ( ; i < AE_CLIPPING_ANTIFLICKER_N; ++i ) {
            p_fsm->targets_history[i] = AE_CLIPPING_HISTORY_INIT_VALUE;
        }
    }
    p_fsm->next_hist_idx = 0;
    p_fsm->max_target = 0;

    ae_roi_update( p_fsm );
}

void ae_read_full_histogram_data( AE_fsm_ptr_t p_fsm )
{
    int i;
    int shift = 0;
    uint32_t sum = 0;
    fsm_param_mon_alg_flow_t ae_flow;

    ae_flow.frame_id_tracking = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    p_fsm->cur_using_stats_frame_id = ae_flow.frame_id_tracking;
    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; ++i ) {
        uint32_t v = acamera_aexp_hist_stats_mem_array_data_read( p_fsm->cmn.isp_base, i );

        shift = ( v >> 12 ) & 0xF;
        v = v & 0xFFF;
        if ( shift ) {
            v = ( v | 0x1000 ) << ( shift - 1 );
        }
        p_fsm->fullhist[i] = v;
        sum += v;
    }
    p_fsm->fullhist_sum = sum;

    ae_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    ae_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AE_FLOW, &ae_flow, sizeof( ae_flow ) );
    LOG( LOG_INFO, "AE flow: INPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", ae_flow.frame_id_tracking, ae_flow.frame_id_current );

#if FW_ZONE_AE
    int j;
    for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ ) {
        for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ ) {
            p_fsm->hist4[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = ( uint16_t )( acamera_metering_mem_array_data_read( p_fsm->cmn.isp_base, ( i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j ) * 2 + 1 ) >> 16 );
        }
    }
#endif
}

void AE_fsm_process_interrupt( AE_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;
    switch ( irq_event ) {
    case ACAMERA_IRQ_AE_STATS:
        ae_read_full_histogram_data( (AE_fsm_ptr_t)p_fsm );
        fsm_raise_event( p_fsm, event_id_ae_stats_ready );
        break;
    }
}

static inline uint32_t adjacent_ratio_to_full( const fsm_param_sensor_info_t *sensor_info, uint32_t ratio )
{
    switch ( sensor_info->sensor_exp_number ) {
    case 4:
        return ( ratio * ratio * ratio ) << 6;
        break;
    case 3:
        return ( ratio * ratio ) << 6;
        break;
    default:
    case 2:
        return ratio << 6;
        break;
    }
}

static inline uint32_t full_ratio_to_adjaced( const fsm_param_sensor_info_t *sensor_info, uint32_t ratio )
{
    switch ( sensor_info->sensor_exp_number ) {
    case 4:
        return acamera_math_exp2( acamera_log2_fixed_to_fixed( ratio, 6, 8 ) / 3, 8, 6 ) >> 6;
        break;
    case 3:
        return acamera_sqrt32( ratio >> 6 );
        break;
    default:
    case 2:
        return ratio >> 6;
        break;
    }
}

uint32_t ae_calculate_exposure_ratio( AE_fsm_ptr_t p_fsm )
{
    ae_balanced_param_t *param = (ae_balanced_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL );
    cmos_control_param_t *param_cmos = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    uint32_t exp_ratio = 64;
    uint32_t max_clip_exp_ratio = 64;

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    const uint32_t max_exposure_ratio = adjacent_ratio_to_full( &sensor_info, param_cmos->global_max_exposure_ratio );

    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_exposure_ratio ) {
        exp_ratio = adjacent_ratio_to_full( &sensor_info, ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_exposure_ratio );
    } else {

#if defined( CALIBRATION_EXPOSURE_RATIO_ADJUSTMENT ) && ( defined( ISP_HAS_IRIDIX8_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM ) )
        //Modulation of clipped pixels according to contrast of scene
        uint32_t iridix_contrast = 256;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_IRIDIX_CONTRAST, NULL, 0, &iridix_contrast, sizeof( iridix_contrast ) );

        uint32_t calibration_exposure_ratio_adjustment_idx = CALIBRATION_EXPOSURE_RATIO_ADJUSTMENT;
        uint32_t er_contrast_adj = acamera_calc_modulation_u16( iridix_contrast, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), calibration_exposure_ratio_adjustment_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), calibration_exposure_ratio_adjustment_idx ) );

        //uint32_t max_clipped_amount = (uint32_t)((uint64_t)p_fsm->fullhist_sum*param->long_clip>>8); //without modulation
        uint32_t long_clip = ( uint32_t )( param->long_clip * er_contrast_adj ) >> 8;
        uint32_t max_clipped_amount = ( uint32_t )( (uint64_t)p_fsm->fullhist_sum * long_clip >> 8 ); //without modulation
                                                                                                      // LOG( LOG_CRIT, " iridix_contrast %d", (int)iridix_contrast);
#else
        uint32_t max_clipped_amount = ( uint32_t )( (uint64_t)p_fsm->fullhist_sum * param->long_clip >> 8 );
#endif

        uint32_t inx = ISP_FULL_HISTOGRAM_SIZE - 1;
        uint32_t clipped_amount = p_fsm->fullhist[inx];

        while ( clipped_amount <= max_clipped_amount && inx > 0 ) { //kernel ooops
            inx--;
            clipped_amount += p_fsm->fullhist[inx];
        }
        exp_ratio = ( ( uint32_t )( ISP_FULL_HISTOGRAM_SIZE - 1 ) << 7 ) / ( 2 * inx + 1 );
        max_clip_exp_ratio = exp_ratio;
        //LOG( LOG_ERR, " exp_ratio CLIP %d", exp_ratio );
        if ( sensor_info.sensor_exp_number > 1 ) {
            const uint32_t *hist = p_fsm->fullhist;

            // digital gain = dg_num / dg_den
            uint32_t dg_num = 1;
            uint32_t dg_den = 1;

#if defined( KERNEL_MODULE ) && ( KERNEL_MODULE != 0 )
            {
                exposure_set_t exp_set_fr_prev;
                int32_t frame_prev = 0;
                acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame_prev, sizeof( int32_t ), &exp_set_fr_prev, sizeof( exp_set_fr_prev ) );

                dg_num = acamera_math_exp2( exp_set_fr_prev.info.isp_dgain_log2, LOG2_GAIN_SHIFT, LOG2_GAIN_SHIFT );
                dg_den = ( 1 << LOG2_GAIN_SHIFT );
            }
#endif
            // Compute partial sums
            {
                p_fsm->WSNR_cumsum[0] = hist[0] * acamera_sqrt32( 64 );
                uint32_t i = 1;
                for ( ; i < ISP_FULL_HISTOGRAM_SIZE; ++i ) {
                    p_fsm->WSNR_cumsum[i] = p_fsm->WSNR_cumsum[i - 1] + hist[i] * acamera_sqrt32( 64 * ( i + 1 ) );
                }
            }

            uint32_t ER_best = ( 1 << AE_FXPOINT_QBITS );
            uint64_t WSNR_best = p_fsm->WSNR_cumsum[ISP_FULL_HISTOGRAM_SIZE - 1];

            switch ( sensor_info.sensor_exp_number ) {
            case 2: {
                int32_t er_cand = 1;
                for ( ; er_cand < 16; ++er_cand ) {
                    const int32_t ER_candidate_sqr = ( er_cand << AE_FXPOINT_QBITS );

                    uint32_t clip_medium = ( ISP_FULL_HISTOGRAM_SIZE * dg_num ) / ( er_cand * dg_den );
                    clip_medium = MAX( 1, clip_medium );
                    clip_medium = MIN( clip_medium, ISP_FULL_HISTOGRAM_SIZE - 1 );

                    // Unsigned because cumsums are monotonic
                    uint64_t current_WSNR = ( ( p_fsm->WSNR_cumsum[ISP_FULL_HISTOGRAM_SIZE - 1] - p_fsm->WSNR_cumsum[clip_medium] ) << AE_FXPOINT_QBITS ) +
                                            ( p_fsm->WSNR_cumsum[clip_medium] * ER_candidate_sqr );

                    if ( current_WSNR > WSNR_best ) {
                        WSNR_best = current_WSNR;
                        ER_best = ( ER_candidate_sqr * ER_candidate_sqr ) >> AE_FXPOINT_QBITS;
                    }
                }
                exp_ratio = ER_best;
            } break;

            case 3: {
                int32_t er_cand = 1;
                for ( ; er_cand < 16; ++er_cand ) {
                    const int32_t ER_candidate_sqr = ( er_cand << AE_FXPOINT_QBITS );

                    uint32_t clip_medium = ( ISP_FULL_HISTOGRAM_SIZE * dg_num ) / ( er_cand * dg_den );
                    uint32_t clip_long = clip_medium / er_cand;

                    clip_long = MAX( 1, clip_long );
                    clip_long = MIN( clip_long, ISP_FULL_HISTOGRAM_SIZE - 2 );

                    clip_medium = MAX( clip_long + 1, clip_medium );
                    clip_medium = MIN( clip_medium, ISP_FULL_HISTOGRAM_SIZE - 1 );

                    // Unsigned because cumsums are monotonic
                    uint64_t current_WSNR = ( ( p_fsm->WSNR_cumsum[ISP_FULL_HISTOGRAM_SIZE - 1] - p_fsm->WSNR_cumsum[clip_medium] ) << AE_FXPOINT_QBITS ) +
                                            ( acamera_sqrt32( ER_candidate_sqr << AE_FXPOINT_QBITS ) * ( p_fsm->WSNR_cumsum[clip_medium] - p_fsm->WSNR_cumsum[clip_long] ) ) +
                                            ( p_fsm->WSNR_cumsum[clip_long] * ER_candidate_sqr );

                    if ( current_WSNR > WSNR_best ) {
                        WSNR_best = current_WSNR;
                        ER_best = ( ER_candidate_sqr * ER_candidate_sqr ) >> AE_FXPOINT_QBITS;
                    }
                }
                exp_ratio = ER_best;
            } break;

            case 4: {
                int32_t er_cand = 1;
                for ( ; er_cand < 16; ++er_cand ) {
                    const int32_t ER_candidate_sqr = ( er_cand << AE_FXPOINT_QBITS );

                    uint32_t clip_medium = ( ISP_FULL_HISTOGRAM_SIZE * dg_num ) / ( er_cand * dg_den );
                    uint32_t clip_medium2 = clip_medium / er_cand;
                    uint32_t clip_long = clip_medium2 / er_cand;

                    clip_long = MAX( 1, clip_long );
                    clip_long = MIN( clip_long, ISP_FULL_HISTOGRAM_SIZE - 3 );

                    clip_medium2 = MAX( clip_long + 1, clip_medium2 );
                    clip_medium2 = MIN( clip_medium2, ISP_FULL_HISTOGRAM_SIZE - 2 );

                    clip_medium = MAX( clip_medium2 + 1, clip_medium );
                    clip_medium = MIN( clip_medium, ISP_FULL_HISTOGRAM_SIZE - 1 );

                    const uint32_t canditade_sqr_root = acamera_sqrt32( ER_candidate_sqr << AE_FXPOINT_QBITS );
                    const uint32_t canditade_cube_root = acamera_math_exp2( acamera_log2_fixed_to_fixed( ER_candidate_sqr, AE_FXPOINT_QBITS, AE_FXPOINT_QBITS ) / 3, AE_FXPOINT_QBITS, AE_FXPOINT_QBITS );

                    // Unsigned because cumsums are monotonic
                    uint64_t current_WSNR = ( ( p_fsm->WSNR_cumsum[ISP_FULL_HISTOGRAM_SIZE - 1] - p_fsm->WSNR_cumsum[clip_medium] ) << AE_FXPOINT_QBITS ) +
                                            ( canditade_cube_root * ( p_fsm->WSNR_cumsum[clip_medium] - p_fsm->WSNR_cumsum[clip_medium2] ) ) +
                                            ( canditade_sqr_root * ( p_fsm->WSNR_cumsum[clip_medium2] - p_fsm->WSNR_cumsum[clip_long] ) ) +
                                            ( ER_candidate_sqr * p_fsm->WSNR_cumsum[clip_long] );

                    if ( current_WSNR > WSNR_best ) {
                        WSNR_best = current_WSNR;
                        ER_best = ( ER_candidate_sqr * ER_candidate_sqr ) >> AE_FXPOINT_QBITS;
                    }
                }
                exp_ratio = ER_best;
            } break;

            default: {
                LOG( LOG_ERR, "Unsupported exposures number!" );
            } break;
            }

            exp_ratio = exp_ratio > max_clip_exp_ratio ? max_clip_exp_ratio : exp_ratio;
            //LOG( LOG_ERR, " ER_best %d max %d ER %d", ER_best>>6, max_clip_exp_ratio>>6, exp_ratio>>6 );
        }
    }
    if ( exp_ratio < 64 ) {
        exp_ratio = 64;
    }
    if ( exp_ratio > max_exposure_ratio ) {
        exp_ratio = max_exposure_ratio;
    }
    if ( param->er_avg_coeff > 0 ) {
        p_fsm->exposure_ratio_avg += exp_ratio - ( p_fsm->exposure_ratio_avg / param->er_avg_coeff ); // division by zero is checked
        exp_ratio = p_fsm->exposure_ratio_avg / param->er_avg_coeff;                                  // division by zero is checked
    }
    if ( !ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_exposure_ratio ) {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_exposure_ratio = full_ratio_to_adjaced( &sensor_info, exp_ratio );
    }

    return exp_ratio;
}

void ae_calculate_target( AE_fsm_ptr_t p_fsm )
{
    ae_balanced_param_t *param = (ae_balanced_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL );
    uint8_t ae_comp = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_ae_compensation;
    if ( p_fsm->fullhist_sum == 0 ) {
        LOG( LOG_WARNING, "FULL HISTOGRAM SUM IS ZERO" );
        return;
    }

    // calculate mean value
    uint32_t cnt;
    uint64_t acc = 0;
    uint32_t n = param->tail_weight;
    uint32_t m1;

    for ( cnt = 0; cnt < ISP_FULL_HISTOGRAM_SIZE - n; cnt++ ) {
        acc += ( 2 * cnt + 1ull ) * p_fsm->fullhist[cnt];
    }
    for ( ; cnt < ISP_FULL_HISTOGRAM_SIZE; cnt++ ) {
        acc += ( cnt - ( ISP_FULL_HISTOGRAM_SIZE - n ) + 2 ) / 2 * ( 2 * cnt + 1ull ) * p_fsm->fullhist[cnt];
    }
    m1 = ( uint32_t )( div64_u64( acc, 2 * p_fsm->fullhist_sum ) );

    p_fsm->ae_hist_mean = m1;

    // get HDR target -> this will prevent clipping
    int32_t total_gain = 0;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

    uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );

    // LOG( LOG_NOTICE, "log2_gain %d total_gain %d", log2_gain, total_gain );

    const int32_t ldr_target = param->target_point;
    const int32_t hdr_target = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL_HDR_TARGET ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL_HDR_TARGET ) );

    LOG( LOG_DEBUG, "hdr_target %d log2_gain %d", hdr_target, log2_gain );

    const uint32_t hi_target_prc = MIN( _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[5], 100 );
    const uint32_t hi_target_p = MIN( _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[6], 100 );
    const uint32_t clip_target = ( hi_target_prc * (uint64_t)p_fsm->fullhist_sum ) / 100;
    const uint32_t unclipped_point = ( hi_target_p * ISP_FULL_HISTOGRAM_SIZE ) / 100;

    // Find a clip point satisfying clipping thresholds
    uint32_t target_pc_point = 0;
    {
        uint32_t pc_counted = 0;
        for ( ; ( target_pc_point < ISP_FULL_HISTOGRAM_SIZE ) && ( pc_counted < clip_target ); ++target_pc_point ) {
            pc_counted += p_fsm->fullhist[target_pc_point];
        }
        target_pc_point = MAX( 1, target_pc_point ); // target_pc_point >= 1
    }

    const int is_unknown_target = ( target_pc_point >= ISP_FULL_HISTOGRAM_SIZE - 1 ) ? 1 : 0; // bool
    if ( is_unknown_target ) {
        p_fsm->targets_history[p_fsm->next_hist_idx++] = 0;
        if ( p_fsm->next_hist_idx >= AE_CLIPPING_ANTIFLICKER_N ) {
            p_fsm->next_hist_idx = 0;
        }

        uint32_t old_target = hdr_target;
        int32_t i = 0;
        int32_t rIdx = p_fsm->next_hist_idx;
        for ( ; i < AE_CLIPPING_ANTIFLICKER_N; ++i ) {
            rIdx = ( rIdx > 0 ) ? rIdx - 1 : AE_CLIPPING_ANTIFLICKER_N - 1;
            if ( p_fsm->targets_history[rIdx] > 0 ) {
                old_target = p_fsm->targets_history[rIdx];
                break;
            }
        }
        // Not using formula X + a * (Y - X) because of unsigned arguments
        // max_target = max_target * 0.8 + max_target_n * 0.2;
        p_fsm->max_target = ( 819 * p_fsm->max_target + 205 * old_target ) >> 10;
    } else {
        const uint32_t mean = ( m1 > 0 ? m1 : 1 );
        const uint32_t target_candidate = ( ( 973 * mean * unclipped_point ) / target_pc_point ) >> 10; // division by zero is checked
                                                                                                        // 0.95 * 1024 = 973
        p_fsm->targets_history[p_fsm->next_hist_idx++] = target_candidate;
        if ( p_fsm->next_hist_idx >= AE_CLIPPING_ANTIFLICKER_N ) {
            p_fsm->next_hist_idx = 0;
        }

        p_fsm->max_target = target_candidate;
    }
    // clamp
    p_fsm->max_target = MAX( p_fsm->max_target, hdr_target );
    p_fsm->max_target = MIN( p_fsm->max_target, ldr_target );
    const int32_t new_target = p_fsm->max_target;

    p_fsm->error_log2 = acamera_log2_fixed_to_fixed( new_target, 0, LOG2_GAIN_SHIFT ) - acamera_log2_fixed_to_fixed( m1, 0, LOG2_GAIN_SHIFT ) +
                        ( ( ( int32_t )(ae_comp)-0x80 ) << ( LOG2_GAIN_SHIFT - 6 + 1 ) ) + ( ( (int32_t)get_exposure_correction( p_fsm ) - 0x80 ) << ( LOG2_GAIN_SHIFT - 6 + 1 ) );

    int32_t p_para = 0, n_para = 0;
    if ( param->AE_tol != 0 ) {
        p_para = acamera_log2_fixed_to_fixed( new_target, 0, LOG2_GAIN_SHIFT ) - acamera_log2_fixed_to_fixed( new_target - param->AE_tol, 0, LOG2_GAIN_SHIFT );
        n_para = acamera_log2_fixed_to_fixed( new_target, 0, LOG2_GAIN_SHIFT ) - acamera_log2_fixed_to_fixed( new_target + param->AE_tol, 0, LOG2_GAIN_SHIFT );
        if ( ( p_fsm->error_log2 <= p_para ) && ( p_fsm->error_log2 >= n_para ) ) {
            p_fsm->error_log2 = 0;
        }
        //LOG( LOG_ERR, "AE_tol  %d p_ %d n_ %d new_target %d p_fsm->error_log2 %d", param->AE_tol, p_para, n_para, new_target, p_fsm->error_log2 );
        //LOG( LOG_ERR, "%ld,%d,%ld,%ld,%d,%d\n", m1, new_target, ldr_target, p_fsm->error_log2, ae_comp, get_exposure_correction( p_fsm ) );
    }

#if FW_ZONE_AE
    uint32_t mean = 0;
    if ( p_fsm->smart_zone_enable ) {
        uint32_t i = 0, j = 0;
        for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ )
            for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ )
                mean += p_fsm->hist4[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j];
        mean /= ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT;
        for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ )
            for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ ) {
                if ( i < p_fsm->y1 || i > p_fsm->y2 || j < p_fsm->x1 || j > p_fsm->x2 ) {
                    p_fsm->zone_weight[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = 0;
                } else {
                    uint16_t h = p_fsm->hist4[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j];
                    if ( ( mean == 0xFFFF ) || ( h <= mean ) )
                        p_fsm->zone_weight[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = 15;
                    else
                        p_fsm->zone_weight[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = 0xF - ( uint8_t )( ( h - mean ) >> 12 );
                }
            }
    } else {
        uint32_t i = 0, j = 0;
        for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ )
            for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ )
                if ( i < p_fsm->y1 || i > p_fsm->y2 || j < p_fsm->x1 || j > p_fsm->x2 ) {
                    p_fsm->zone_weight[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = 0;
                } else {
                    p_fsm->zone_weight[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = 15;
                }
    }
#endif
}

void ae_calculate_exposure( AE_fsm_ptr_t p_fsm )
{
    int32_t exposure_log2 = 0;
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_exposure ) {
        exposure_log2 = acamera_log2_fixed_to_fixed( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_exposure, 6, LOG2_GAIN_SHIFT );
        if ( exposure_log2 < 0 )
            exposure_log2 = 0;
    } else {
        ae_balanced_param_t *param = (ae_balanced_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL );
        int32_t coeff = param->pi_coeff;
        int64_t max_exposure;
        int32_t max_exposure_log2 = 0;

        int32_t type = CMOS_MAX_EXPOSURE_LOG2;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_LOG2, &type, sizeof( type ), &max_exposure_log2, sizeof( max_exposure_log2 ) );
        max_exposure = (int64_t)coeff * max_exposure_log2;
        p_fsm->integrator += p_fsm->error_log2;

        if ( p_fsm->integrator < 0 )
            p_fsm->integrator = 0;
        if ( p_fsm->integrator > max_exposure )
            p_fsm->integrator = max_exposure;

        exposure_log2 = (int32_t)div64_s64( p_fsm->error_log2 + p_fsm->integrator, coeff );

        LOG( LOG_DEBUG, "exp: %d, err_log2: %d, integrator: %d, coeff: %d\n", (int)exposure_log2, (int)p_fsm->error_log2, (int)p_fsm->integrator, (int)coeff );
        if ( exposure_log2 < 0 )
            exposure_log2 = 0;
        LOG( LOG_DEBUG, "max_exposure_log2: %d, max_exposure: %d, exposure_log2: %d\n", (int)max_exposure_log2, (int)max_exposure, (int)exposure_log2 );

        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_exposure = ( acamera_math_exp2( exposure_log2, LOG2_GAIN_SHIFT, 6 ) );
    }

    p_fsm->exposure_log2 = exposure_log2;
    p_fsm->exposure_ratio = ae_calculate_exposure_ratio( p_fsm );

    fsm_param_exposure_target_t exp_target;
    exp_target.exposure_log2 = exposure_log2;
    exp_target.exposure_ratio = p_fsm->exposure_ratio;
    exp_target.frame_id_tracking = p_fsm->cur_using_stats_frame_id;

    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_EXPOSURE_TARGET, &exp_target, sizeof( exp_target ) );

    // skip when frame_id is 0.
    if ( p_fsm->cur_using_stats_frame_id ) {
        fsm_param_mon_alg_flow_t ae_flow;
        ae_flow.frame_id_tracking = p_fsm->cur_using_stats_frame_id;
        ae_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        ae_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AE_FLOW, &ae_flow, sizeof( ae_flow ) );
        LOG( LOG_INFO, "AE flow: OUTPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", ae_flow.frame_id_tracking, ae_flow.frame_id_current );
    }

    LOG( LOG_DEBUG, "ae exposure_log2: %d.", (int)exposure_log2 );
}
void ae_exposure_correction( AE_fsm_ptr_t p_fsm, int32_t corr )
{
    ae_balanced_param_t *param = (ae_balanced_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL );
    p_fsm->integrator += corr * param->pi_coeff;
}
