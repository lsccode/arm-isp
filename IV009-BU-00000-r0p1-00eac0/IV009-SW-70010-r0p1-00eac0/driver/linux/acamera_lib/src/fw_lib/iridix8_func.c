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
#include "acamera_math.h"

#include "acamera_command_api.h"
#include "iridix8_fsm.h"
#if IRIDIX_HAS_PRE_POST_GAMMA_LUT_LINEAR
#include "acamera_iridix_fwd_mem_config.h"
#include "acamera_iridix_rev_mem_config.h"
#endif


#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_IRIDIX8
#endif


static void iridix_mointor_frame_end( iridix_fsm_ptr_t p_fsm )
{

    uint32_t irq_mask = acamera_isp_isp_global_interrupt_status_vector_read( p_fsm->cmn.isp_base );
    if ( irq_mask & 0x1 ) {
        fsm_param_mon_err_head_t mon_err;
        mon_err.err_type = MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_ERROR_REPORT, &mon_err, sizeof( mon_err ) );
    }
}

void iridix_fsm_process_interrupt( iridix_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    // Get and process interrupts
    if ( irq_event == ACAMERA_IRQ_FRAME_END ) {

        int32_t diff = 256;
        uint8_t iridix_avg_coeff = _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0];

        int32_t diff_iridix_DG = 256;
        uint16_t iridix_global_DG = p_fsm->iridix_global_DG;

        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
            acamera_isp_iridix_gain_gain_write( p_fsm->cmn.isp_base, iridix_global_DG );
        }
        diff_iridix_DG = ( ( (int32_t)iridix_global_DG ) << 8 ) / ( int32_t )( p_fsm->iridix_global_DG_prev );


        iridix_avg_coeff = _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0];

        exposure_set_t exp_set_fr_next;
        exposure_set_t exp_set_fr_prev;
        int32_t frame_next = 1, frame_prev = 2;
// int32_t frame_next = 1, frame_prev = 0;
#if ISP_HAS_TEMPER == 3
        if ( !acamera_isp_temper_temper2_mode_read( p_fsm->cmn.isp_base ) ) { // temper 3 has 1 frame delay
            frame_next = 0;
            frame_prev = -1;
        }
#endif

        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame_next, sizeof( frame_next ), &exp_set_fr_next, sizeof( exp_set_fr_next ) );
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame_prev, sizeof( frame_prev ), &exp_set_fr_prev, sizeof( exp_set_fr_prev ) );

        diff = acamera_math_exp2( exp_set_fr_prev.info.exposure_log2 - exp_set_fr_next.info.exposure_log2, LOG2_GAIN_SHIFT, 8 );
        // LOG(LOG_DEBUG,"diff1 %d %d ", (int)diff,(int)diff_iridix_DG);
        diff = ( diff * diff_iridix_DG ) >> 8;

        // LOG(LOG_DEBUG,"diff2 %d\n", (int)diff);
        if ( diff < 0 )
            diff = 256;
        if ( diff >= ( 1 << ACAMERA_ISP_IRIDIX_COLLECTION_CORRECTION_DATASIZE ) )
            diff = ( 1 << ACAMERA_ISP_IRIDIX_COLLECTION_CORRECTION_DATASIZE );
        //LOG(LOG_DEBUG,"%u %ld prev %ld\n", (unsigned int)diff, exp_set_fr1.info.exposure_log2, exp_set_fr0.info.exposure_log2);
        // LOG(LOG_DEBUG,"diff3 %d\n", (int)diff);

        acamera_isp_iridix_collection_correction_write( p_fsm->cmn.isp_base, diff );

        diff = 256; // this logic diff = 256 where strength is only updated creates a long delay.
        if ( diff == 256 ) {
            // time filter for iridix strength
            uint16_t iridix_strength = p_fsm->strength_target;
            if ( iridix_avg_coeff > 1 ) {
                ( (iridix_fsm_ptr_t)p_fsm )->strength_avg += p_fsm->strength_target - p_fsm->strength_avg / iridix_avg_coeff; // division by zero is checked
                iridix_strength = ( uint16_t )( p_fsm->strength_avg / iridix_avg_coeff );                                     // division by zero is checked
            }
            if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
                acamera_isp_iridix_strength_inroi_write( p_fsm->cmn.isp_base, iridix_strength >> 6 );
            }

            if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
                acamera_isp_iridix_dark_enh_write( p_fsm->cmn.isp_base, p_fsm->dark_enh );
            }
        }

        if ( p_fsm->frame_id_tracking ) {
            fsm_param_mon_alg_flow_t iridix_flow;

            iridix_flow.frame_id_tracking = p_fsm->frame_id_tracking;
            iridix_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &( (iridix_fsm_t *)p_fsm )->cmn );
            iridix_flow.flow_state = MON_ALG_FLOW_STATE_APPLIED;
            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_IRIDIX_FLOW, &iridix_flow, sizeof( iridix_flow ) );
            LOG( LOG_INFO, "Iridix8 flow: APPLIED: frame_id_tracking: %d, cur frame_id: %u.", iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );
            ( (iridix_fsm_ptr_t)p_fsm )->frame_id_tracking = 0;
        }

        iridix_mointor_frame_end( (iridix_fsm_ptr_t)p_fsm );
    }
}


void iridix_initialize( iridix_fsm_t *p_fsm )
{
    uint16_t i;
    //put initialization here
    p_fsm->strength_avg = IRIDIX_STRENGTH_TARGET * _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0];

    // Initialize parameters
    if ( _GET_WIDTH( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY ) == sizeof( int32_t ) ) {
        // 32 bit tables
        for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY ); i++ ) {
            uint32_t val = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY )[i];
            acamera_isp_iridix_lut_asymmetry_lut_write( p_fsm->cmn.isp_base, i, val );
        }
    } else {
        // 16 bit tables
        for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY ); i++ ) {
            acamera_isp_iridix_lut_asymmetry_lut_write( p_fsm->cmn.isp_base, i, _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY )[i] );
        }
    }

    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->stab.global_iridix_strength_target = ( p_fsm->strength_target );

#if ISP_WDR_SWITCH
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

    if ( wdr_mode == WDR_MODE_LINEAR ) {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_maximum_iridix_strength = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_STRENGTH_MAXIMUM )[0] );

    } else if ( wdr_mode == WDR_MODE_FS_LIN ) {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_maximum_iridix_strength = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_STRENGTH_MAXIMUM )[0] );
    } else
#endif
    {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_maximum_iridix_strength = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_STRENGTH_MAXIMUM )[0] );
    }

    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END );
    iridix_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );
}

uint16_t iridix_control_strength_clip_calculate( iridix_fsm_t *p_fsm )
{
    // Calculate iridix strength based on the histogram of the current image
    int16_t ir_str;
    int32_t max_str, max_drk;
    int32_t cmos_exposure_log2 = 0;

    int32_t type = CMOS_CURRENT_EXPOSURE_LOG2;
    fsm_param_ae_hist_info_t ae_hist_info;
    fsm_param_ae_info_t ae_info;
    // init to NULL in case no AE configured and caused wrong access via wild pointer
    ae_hist_info.fullhist = NULL;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_LOG2, &type, sizeof( type ), &cmos_exposure_log2, sizeof( cmos_exposure_log2 ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AE_HIST_INFO, NULL, 0, &ae_hist_info, sizeof( ae_hist_info ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AE_INFO, NULL, 0, &ae_info, sizeof( ae_info ) );


    ir_str = p_fsm->mp_iridix_strength;

    uint32_t ev_lim_no_str = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_NO_STR )[0];
    if ( cmos_exposure_log2 < _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] ) {
        max_str = 100;
    } else if ( ev_lim_no_str < cmos_exposure_log2 ) {
        max_str = 0;
    } else {
        int tmp_ev_lim = ( ev_lim_no_str - _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] );
        if ( tmp_ev_lim == 0 ) {
            tmp_ev_lim = 1;
        }
        max_str = 100 - ( 100 * ( cmos_exposure_log2 - _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] ) ) / tmp_ev_lim;
    }

    if ( max_str < _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_MIN_MAX_STR )[0] && cmos_exposure_log2 < _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] )
        max_str = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_MIN_MAX_STR )[0];

    if ( max_str < 0 )
        max_str = 0;

    if ( ir_str > max_str ) {
        ir_str = max_str;
    }
    //////////////////////////// clip dk according to ev
    uint32_t ev_lim_no_drk = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_NO_STR )[1];

    if ( cmos_exposure_log2 < _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] ) {
        max_drk = 100;
    } else if ( ev_lim_no_drk < cmos_exposure_log2 ) {
        max_drk = 0;
    } else {
        int tmp_drk = ( ev_lim_no_drk - _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] );
        if ( tmp_drk == 0 ) {
            tmp_drk = 1;
        }
        max_drk = 100 - ( 100 * ( cmos_exposure_log2 - _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] ) ) / tmp_drk;
    }

    if ( max_drk < 0 ) max_drk = 0;

    //Clip dark enh
    int32_t min_dk = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[2];
    int32_t max_dk = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[3];
    int32_t m = ( ( min_dk - max_dk ) ) / ( 0 - 100 );
    uint32_t dk_enh_clip = min_dk + m * ( max_drk - 0 );
    if ( cmos_exposure_log2 > _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_EV_LIM_FULL_STR )[0] ) {
        p_fsm->dark_enh = p_fsm->dark_enh > dk_enh_clip ? dk_enh_clip : p_fsm->dark_enh;
    }
    p_fsm->dark_enh = p_fsm->dark_enh > dk_enh_clip ? dk_enh_clip : p_fsm->dark_enh;

    uint32_t smin = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_minimum_iridix_strength;
    uint32_t smax = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_maximum_iridix_strength;
    uint32_t strength = 256 * smin + ( 256 * ( smax - smin ) * ir_str ) / 100;

    if ( p_fsm->frame_id_tracking ) {
        fsm_param_mon_alg_flow_t iridix_flow;

        iridix_flow.frame_id_tracking = p_fsm->frame_id_tracking;
        iridix_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        iridix_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_IRIDIX_FLOW, &iridix_flow, sizeof( iridix_flow ) );
        LOG( LOG_INFO, "Iridix flow: OUTPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );
    }

    return ( (uint16_t)strength );
}

uint16_t iridix_control_dark_enhancement_and_strength_calculate( iridix_fsm_t *p_fsm )
{
    // Calculate iridix strength based on the histogram of the current image
    fsm_param_ae_hist_info_t ae_hist_info;
    fsm_param_ae_info_t ae_info;

    // init to NULL in case no AE configured and caused wrong access via wild pointer
    ae_hist_info.fullhist = NULL;

    int32_t i;
    int32_t pD;
    int32_t pH;
    int32_t chist;
    int32_t m;
    int32_t pD_cut = 0;
    int32_t pH_cut = 0;
    int32_t dk_enh = 0;
    int32_t ir_strength_max_ratio = 100;
    int32_t ir_strength;
    int32_t contrast;
    int32_t dk_target;
    int32_t ir_gain;


    int32_t dark_prc = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[0];
    int32_t bright_prc = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[1];
    //dark enhancement parameters
    int32_t min_dk = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[2];
    int32_t max_dk = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[3];
    int32_t pD_cut_min = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[4];
    int32_t pD_cut_max = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[5];
    int32_t dark_contrast_min = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[6];
    int32_t dark_contrast_max = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[7];
    int32_t median = 0;
    //Iridix strength parameters
    int32_t min_str = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[8];
    int32_t max_str = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[9];
    int32_t dark_prc_gain_target = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[10];
    int32_t contrast_min = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[11];    //U24.8 tuning parameter
    int32_t contrast_max = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[12];    //U24.8 tuning parameter
    int32_t iridix_max_gain = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL )[13]; //U24.8 tuning parameter

    int32_t target_LDR = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[1]; // This should correspond to 0.48 point in RAW histogram so that mean intensity value after gamma correction is 18% grey (122 in 0:255 histogram)

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AE_HIST_INFO, NULL, 0, &ae_hist_info, sizeof( ae_hist_info ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AE_INFO, NULL, 0, &ae_info, sizeof( ae_info ) );

    //pH, and pD are the percentage of pixels for dark and highlight
    pD = ( (uint64_t)ae_hist_info.fullhist_sum * dark_prc ) / 100;
    pH = ( (uint64_t)ae_hist_info.fullhist_sum * bright_prc ) / 100;


    int32_t tpixels_median = ae_hist_info.fullhist_sum / 2; // mid point of histogram

    //calculate median of histogram
    chist = 0;
    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; i++ ) {
        chist += ae_hist_info.fullhist[i];
        if ( tpixels_median < chist ) {
            median = i;
            break;
        }
    }
    //calculate pH_cut - intensity cut for highlight pixels
    chist = 0;
    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; i++ ) {
        chist += ae_hist_info.fullhist[i];
        if ( pH <= chist ) {
            pH_cut = i;
            break;
        }
    }

    //calculate pD - - intensity cut for dark pixels
    chist = 0;
    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; i++ ) {
        chist += ae_hist_info.fullhist[i];
        if ( pD <= chist ) {
            pD_cut = i;
            break;
        }
    }
    pH_cut = ( pH_cut <= 0 ) ? 1 : pH_cut;
    pD_cut = ( pD_cut <= 0 ) ? 1 : pD_cut;

    // Compensate for iridix global digital gain
    pD_cut = ( pD_cut * p_fsm->iridix_global_DG ) >> 8;
    pH_cut = ( pH_cut * p_fsm->iridix_global_DG ) >> 8;
    pH_cut = ( pH_cut <= 0 ) ? 1 : pH_cut;
    pD_cut = ( pD_cut <= 0 ) ? 1 : pD_cut;
    median = ( median * p_fsm->iridix_global_DG ) >> 8;


    /*
        Calculate dark enhancement as a function of intensity cut of dark pixels
                            f(pD_cut)
            max_dk     ___
                    |    \
                    |    .
                    |     \
                    |      .
                    |        \_____________
            min_dk    |
                    |--|----|-------------
                    pD_cut_min   pD_cut_max
    */


    m = 0;
    m = ( ( max_dk << 8 ) - ( min_dk << 8 ) ) / ( ( pD_cut_min != pD_cut_max ) ? pD_cut_min - pD_cut_max : 1 );
    int32_t dk_enh_Icut = ( ( m * ( median - pD_cut_min ) ) + ( max_dk << 8 ) ) >> 8;

    contrast = ( ( pH_cut << 8 ) / pD_cut ); //U24.8
    m = 0;

    if ( median <= 2 ) {
        min_dk = ( max_dk + 4096 ) / 2;
    }
    m = ( ( max_dk - min_dk ) << 8 ) / ( ( dark_contrast_max != dark_contrast_min ) ? dark_contrast_max - dark_contrast_min : 1 ); //U24.8
    dk_enh = ( ( m * ( contrast - dark_contrast_max ) ) + ( max_dk << 8 ) ) >> 8;                                                  //U32.0
    dk_enh = dk_enh_Icut > dk_enh ? dk_enh : dk_enh_Icut;
    if ( dk_enh < min_dk ) {
        dk_enh = min_dk;
    } else if ( dk_enh > max_dk ) {
        dk_enh = max_dk;
    }

    // time filter for iridix dark enhancement
    uint16_t dark_enh_coeff = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0] ) * 2;

    if ( dark_enh_coeff > 1 ) {
        p_fsm->dark_enh_avg += dk_enh - p_fsm->dark_enh_avg / dark_enh_coeff; // division by zero is checked
        dk_enh = ( uint16_t )( p_fsm->dark_enh_avg / dark_enh_coeff );        // division by zero is checked
    }

    //Iridix strength is caclulated as: min(f(pD_cut),f(contrast))
    //------ f(pD_cut)

    dk_target = ISP_FULL_HISTOGRAM_SIZE * dark_prc_gain_target / 100;
    ir_gain = ( ( dk_target << 8 ) / pD_cut ) >> 8; //U32.0

    ir_strength = 100 * ( ir_gain - 1 ) / ( iridix_max_gain - 1 );

    ir_strength = ir_strength > 100 ? 100 : ir_strength;
    ir_strength = ir_strength < 0 ? 0 : ir_strength;

    /*
                        f(contrast)
        max str |                --------
                |             /
                |           -
                |         /
                |      -
                |    /
                |....
        min str    |---|----------|--------
                contrast_min   contrast_max
        */

    //clip strength according to f(contrast)
    if ( contrast <= contrast_min ) {
        ir_strength_max_ratio = min_str;
        if ( median <= target_LDR ) {
            m = 0;
            m = ( ( max_str - min_str ) << 8 ) / ( target_LDR ? -target_LDR : 1 );      //U24.16
            ir_strength_max_ratio = ( ( m * ( median - 0 ) ) + ( max_str << 8 ) ) >> 8; //U32.0
                                                                                        // LOG(LOG_DEBUG,"a %d \n", (int)ir_strength_max_ratio);
        }

    } else if ( contrast >= contrast_max ) {
        ir_strength_max_ratio = max_str;
        // LOG(LOG_DEBUG,"c %d \n", (int)ir_strength_max_ratio);

    } else {
        if ( pH_cut < target_LDR ) {
            m = 0;
            m = ( ( max_str - min_str ) << 8 ) / ( ( 0 - target_LDR ) );                //U24.16
            ir_strength_max_ratio = ( ( m * ( median - 0 ) ) + ( max_str << 8 ) ) >> 8; //U32.0
                                                                                        // LOG(LOG_DEBUG,"b1 %d \n", (int)ir_strength_max_ratio);

        } else if ( median <= target_LDR ) {
            m = 0;
            m = ( ( max_str - min_str ) << 8 ) / ( target_LDR ? -target_LDR : 1 );      //U24.16
            ir_strength_max_ratio = ( ( m * ( median - 0 ) ) + ( max_str << 8 ) ) >> 8; //U32.0
                                                                                        // LOG(LOG_DEBUG,"b2 %d \n", (int)ir_strength_max_ratio);
        } else {
            m = 0;
            m = ( ( max_str - min_str ) << 8 ) / ( ( contrast_max - contrast_min ) ? ( contrast_max - contrast_min ) : 1 ); //U24.16
            ir_strength_max_ratio = ( ( m * ( contrast - contrast_min ) ) + ( min_str << 8 ) ) >> 8;                        //U32.0
                                                                                                                            // LOG(LOG_DEBUG,"b3 %d \n", (int)ir_strength_max_ratio);
        }
    }

    ir_strength = ir_strength > ir_strength_max_ratio ? ir_strength_max_ratio : ir_strength;

    ir_strength = ir_strength < min_str ? min_str : ir_strength;
    p_fsm->iridix_contrast = contrast;


    p_fsm->mp_iridix_strength = (uint16_t)ir_strength;

    //LOG( LOG_NOTICE, "median %d pD_cut = %d pH_cut = %d contrast = %d irxDG = %d ir_strength_max_ratio %d ir_strength %d dk_enh %d\n", (int)median, (int)pD_cut, (int)pH_cut, (int)contrast >> 8, (int)p_fsm->iridix_global_DG, (int)ir_strength_max_ratio, (int)ir_strength, (int)dk_enh );

    return ( (uint16_t)dk_enh );
}
uint16_t iridix_control_global_DG_calculate( iridix_fsm_t *p_fsm )
{

    /*
    Function Description:
    This function calculates the required digital gain, (independant from AE) so that iridix does not need to be used as a global gain.
    This gain will be set in accordance to contrast of the scene.
*/
    fsm_param_ae_hist_info_t ae_hist_info;
    fsm_param_ae_info_t ae_info;

    // init to NULL in case no AE configured and caused wrong access via wild pointer
    ae_hist_info.fullhist = NULL;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AE_HIST_INFO, NULL, 0, &ae_hist_info, sizeof( ae_hist_info ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AE_INFO, NULL, 0, &ae_info, sizeof( ae_info ) );

    if ( ae_hist_info.frame_id ) {
        fsm_param_mon_alg_flow_t iridix_flow;

        iridix_flow.frame_id_tracking = ae_hist_info.frame_id;
        iridix_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        iridix_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;

        // check whether previous frame_id_tracking finished.
        if ( p_fsm->frame_id_tracking ) {
            LOG( LOG_INFO, "Iridix flow: Overwrite: prev frame_id_tracking: %d, new: %u, cur: %d.", p_fsm->frame_id_tracking, iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );
        }

        p_fsm->frame_id_tracking = iridix_flow.frame_id_tracking;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_IRIDIX_FLOW, &iridix_flow, sizeof( iridix_flow ) );
        LOG( LOG_INFO, "Iridix flow: INPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );
    }

    int32_t chist = 0;
    int32_t gain, i, ir_gdg_enable;
    int32_t pH;        // No. of pixels in cummulative histogram that is less than the intensity cut for highlight pixels. This percentage corresponds to bright_prc
    int32_t pClip_cut; // Intensity cut for highlight/clipped pixels. if clipping should be avoided bright_prc should be set to 100% of pixels
    int32_t max_gain_clipping;
    // ------- Tuning parameters --------- //
    int32_t target_LDR;    // This should correspond to 0.48 point in RAW histogram so that mean intensity value after gamma correction is 18% grey (122 in 0:255 histogram)
    int32_t bright_prc;    //  No of pixels that should be below hi_target_prc
    int32_t hi_target_prc; // Target for the bright_prc pixels to avoide/controll clipping


    //get values from calibration parameters
    target_LDR = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[1];
    bright_prc = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[5];
    hi_target_prc = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[6];
    ir_gdg_enable = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_CONTROL )[7];
    // max clipped gain will make sure there that only 1% of pixels is avobe 75% of histogram
    pH = ( (uint64_t)ae_hist_info.fullhist_sum * bright_prc ) / 100;

    chist = 0;
    pClip_cut = 0;
    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; i++ ) {
        chist += ae_hist_info.fullhist[i];
        if ( pH <= chist ) {
            pClip_cut = i;
            break;
        }
    }

    pClip_cut = ( pClip_cut <= 0 ) ? 1 : pClip_cut;
    max_gain_clipping = ( ( ISP_FULL_HISTOGRAM_SIZE * hi_target_prc / 100 ) << 8 ) / pClip_cut; //U24.8

    //Try to always have a balance target as if it was a low dynamic range scene.
    gain = ( ( target_LDR << 8 ) / ( ae_info.ae_hist_mean ? ae_info.ae_hist_mean : 1 ) ); //U24.8

    // pH pixels should be below hi_target_prc of histogram to avoide clipping
    gain = gain <= max_gain_clipping ? gain : max_gain_clipping;
    gain = gain > 256 ? gain : 256;

    gain = gain > 4095 ? 4095 : gain; //12 bits gain U4.8

    if ( ir_gdg_enable == 0 ) {
        gain = 256;
    }

    p_fsm->iridix_global_DG_prev = p_fsm->iridix_global_DG; // already applyied gain

    // time filter for iridix global DG
    uint8_t iridix_avg_coeff = _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0];
    if ( iridix_avg_coeff > 1 ) {
        iridix_avg_coeff = iridix_avg_coeff;
        p_fsm->iridix_global_DG_avg += gain - p_fsm->iridix_global_DG_avg / iridix_avg_coeff; // division by zero is checked
        gain = ( uint16_t )( p_fsm->iridix_global_DG_avg / iridix_avg_coeff );                // division by zero is checked
    }

    return ( (uint16_t)gain );
}

void iridix_update( iridix_fsm_t *p_fsm )
{
    // Update iridix strength (applied to hardware through iridix_fsm_process_interrupt)
    uint16_t strength_target;
    //p_fsm->dark_enh = iridix_control_dark_enhancement_calculate(p_fsm);
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
        p_fsm->iridix_global_DG = iridix_control_global_DG_calculate( p_fsm );
        p_fsm->dark_enh = iridix_control_dark_enhancement_and_strength_calculate( p_fsm );
        strength_target = iridix_control_strength_clip_calculate( p_fsm );
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_iridix_strength_target = ( strength_target >> 6 );
    } else {
        strength_target = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_iridix_strength_target;
        strength_target <<= 8;
    }
    uint16_t smin = (uint16_t)ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_minimum_iridix_strength * 256;
    uint16_t smax = (uint16_t)ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_maximum_iridix_strength * 256;
    if ( strength_target < smin ) {
        strength_target = smin;
    }
    if ( strength_target > smax ) {
        strength_target = smax;
    }
    p_fsm->strength_target = strength_target;
}
