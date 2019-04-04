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

#include "acamera_types.h"

#include "acamera_fw.h"
#include "acamera_metering_stats_mem_config.h"
#include "acamera_math.h"
#include "acamera_lens_api.h"
#include "system_stdlib.h"
#include "acamera_isp_core_nomem_settings.h"
#include "af_lms_fsm.h"

//================================================================================
//#define TRACE
//#define TRACE_IQ
//#define TRACE_SCENE_CHANGE
//================================================================================

#if defined( ISP_METERING_OFFSET_AF )
#define ISP_METERING_OFFSET_AUTO_FOCUS ISP_METERING_OFFSET_AF
#elif defined( ISP_METERING_OFFSET_AF2W )
#define ISP_METERING_OFFSET_AUTO_FOCUS ISP_METERING_OFFSET_AF2W
#else
#error "The AF metering offset is not defined in acamera_metering_mem_config.h"
#endif


#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AF_LMS
#endif

static __inline void acamera_af2_statistics_data_read( AF_fsm_const_ptr_t p_fsm, uint32_t index_inp, uint32_t *stats )
{
    stats[0] = acamera_metering_stats_mem_array_data_read( p_fsm->cmn.isp_base, ISP_METERING_OFFSET_AUTO_FOCUS + ( ( index_inp ) << 1 ) + 0 );
    stats[1] = acamera_metering_stats_mem_array_data_read( p_fsm->cmn.isp_base, ISP_METERING_OFFSET_AUTO_FOCUS + ( ( index_inp ) << 1 ) + 1 );
}

//================================================================================
static int32_t AF_process_statistic( AF_fsm_ptr_t p_fsm )
{
    uint8_t x, y;
    uint8_t zones_horiz = acamera_isp_metering_af_nodes_used_horiz_read( p_fsm->cmn.isp_base );
    uint8_t zones_vert = acamera_isp_metering_af_nodes_used_vert_read( p_fsm->cmn.isp_base );
    uint32_t( *stats )[2] = p_fsm->zone_raw_statistic;
    uint8_t *weight = p_fsm->zone_weight;
    uint64_t total = 0;
    uint32_t total_weight = 0;
    uint32_t max_I2_order = 0;

    if ( !zones_horiz || !zones_vert ) {
        zones_horiz = ISP_DEFAULT_AF_ZONES_HOR;
        zones_vert = ISP_DEFAULT_AF_ZONES_VERT;
    }
    for ( y = 0; y < zones_vert; y++ ) {
        uint32_t inx = (uint32_t)y * zones_horiz;
        for ( x = 0; x < zones_horiz; x++ ) {
            if ( weight[inx + x] ) {
                uint32_t af_word2 = stats[inx + x][1];
                uint32_t e_I2 = ( af_word2 >> 16 ) & 0x1F;
                if ( e_I2 > max_I2_order ) {
                    max_I2_order = e_I2;
                }
            }
        }
    }

    for ( y = 0; y < zones_vert; y++ ) {
        uint32_t inx = (uint32_t)y * zones_horiz;
        for ( x = 0; x < zones_horiz; x++ ) {
            //-----------------------------------------------------
            //
            //              AF2 statistics data format
            //
            //                  --- WORD 1 ---
            //  ____________________________________________
            //  |_________m_I4 _______|_________m_I2________|
            // 32                    16                     0
            //
            //                  --- WORD 2 ---
            //  ____________________________________________
            //  |_|e_E4_|_e_I4 _|_e_I2_|___________m_E4______|
            //   31    26      21     16                     0
            //
            //  where
            //          m_I2 - mantissa for I^2 (16 bits)
            //          m_I4 - mantissa for I^4 (16 bits)
            //          m_E4 - mantissa for E^4 (16 bits)
            //          e_I2 - exponenta for I^2 (5 bits)
            //          e_I4 - exponenta for I^4 (5+1 bits) -- MSB is 31st bit in word2
            //          e_E4 - exponenta for E^4 (5 bits)
            //
            //----------------------------------------------------
            /*
            Stats:Works on Bayer Domain
            R11G12R13
            G21B22G23
            R31G32R33
            N is the Number of Pixels/Zone.
            I2=N*(SUM_OF(PIXLES^2) in zone).
            I4=N*(SUM_OF(PIXLES^4) in zone) = (I2>>4)>^2
            E4=SUM_OF( Sum of (Diff of adj neighbours).^4)
            For B22 (R13-R31)^4+(R11-R33)^4+(G12-G32)^4+(G21-G23)^4)
            Q Format for m_E4 and m_I4 are matched.
            m_I4 is actually Left shifted by 8.*/

            uint32_t af_word1 = stats[inx + x][0];
            uint32_t af_word2 = stats[inx + x][1];
            uint32_t m_I4 = af_word1 >> 16;
            uint32_t m_E4 = af_word2 & 0x0000FFFF;
            uint32_t e_I2 = ( af_word2 >> 16 ) & 0x1F;
            uint32_t e_I4 = ( ( af_word2 >> 21 ) & 0x1F ) | ( ( af_word2 >> 31 ) << 5 );
            uint32_t e_E4 = ( af_word2 >> 26 ) & 0x1F;

            if ( e_I4 == 0x1F && m_I4 == 0xFFFF ) { // saturation
                LOG( LOG_WARNING, "AF2 saturation" );
            } else {

                if ( e_I4 ) {
                    m_I4 = m_I4 | 0x10000;
                    e_I4 = e_I4 - 1;
                }
                if ( e_E4 ) {
                    m_E4 = m_E4 | 0x10000;
                    e_E4 = e_E4 - 1;
                }

                uint64_t res = 0;
                if ( m_I4 ) {
                    res = ( (uint64_t)m_E4 << ( 32 + e_E4 - e_I4 ) ) / m_I4;
                }
                total += res * weight[inx + x];
                total_weight += weight[inx + x];
                p_fsm->zone_process_statistic[inx + x] = res * weight[inx + x];
                p_fsm->zone_process_reliablility[inx + x] = max_I2_order - e_I2;
            }
        }
    }
    if ( total_weight )
        total /= total_weight;
    if ( total > 0xFFFFFFFF ) {
        LOG( LOG_WARNING, "Total saturation" );
    }
    return acamera_log2_fixed_to_fixed_64( total, 0, LOG2_GAIN_SHIFT );
}
//================================================================================
void AF_fsm_process_interrupt( AF_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;
    uint8_t zones_horiz, zones_vert, x, y;
    uint32_t( *stats )[2];

    //check if lens was initialised properly
    if ( p_fsm->lens_driver_ok == 0 )
        return;

    switch ( irq_event ) {
    case ACAMERA_IRQ_AF2_STATS: // read out the statistic
        zones_horiz = acamera_isp_metering_af_nodes_used_horiz_read( p_fsm->cmn.isp_base );
        zones_vert = acamera_isp_metering_af_nodes_used_vert_read( p_fsm->cmn.isp_base );

        if ( !zones_horiz || !zones_vert ) {
            zones_horiz = ISP_DEFAULT_AF_ZONES_HOR;
            zones_vert = ISP_DEFAULT_AF_ZONES_VERT;
        }
        stats = ( (AF_fsm_ptr_t)p_fsm )->zone_raw_statistic;
        for ( y = 0; y < zones_vert; y++ ) {
            uint32_t inx = (uint32_t)y * zones_horiz;
            for ( x = 0; x < zones_horiz; x++ ) {
                uint32_t full_inx = inx + x;
                acamera_af2_statistics_data_read( p_fsm, full_inx, stats[full_inx] );
            }
        }
        if ( p_fsm->frame_num )
            ( (AF_fsm_ptr_t)p_fsm )->frame_num--;
        fsm_raise_event( p_fsm, event_id_af_stats_ready );
        if ( p_fsm->mode != AF_MODE_CALIBRATION && p_fsm->af_status == AF_STATUS_FAST_SEARCH && p_fsm->skip_frame <= 1 ) {
            const af_fast_search_param_t *fs = &p_fsm->fs;
            uint8_t step = fs->step + 1;
            uint16_t position;
            if ( step <= fs->step_number ) {
                position = p_fsm->pos_inf + ( uint16_t )( ( uint32_t )( p_fsm->pos_macro - p_fsm->pos_inf ) * ( step - 1 ) / ( fs->step_number - 1 ) );
            } else {
                position = p_fsm->pos_max;
            }

            if ( p_fsm->lens_ctrl.move ) {
                p_fsm->lens_ctrl.move( p_fsm->lens_ctx, position );
            }
        }
        break;
    }
}
//================================================================================
void AF_init( AF_fsm_ptr_t p_fsm )
{
    af_lms_param_t *param = NULL;
    //check if lens was initialised properly
    if ( !p_fsm->lens_driver_ok ) {
#if USER_MODULE
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_KLENS_STATUS, NULL, 0, &p_fsm->lens_driver_ok, sizeof( p_fsm->lens_driver_ok ) );
        if ( p_fsm->lens_driver_ok ) {
            param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
            acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_KLENS_PARAM, NULL, 0, &p_fsm->lens_param, sizeof( p_fsm->lens_param ) );
        }
#else
        p_fsm->lens_ctx = NULL;

        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.lens_init != NULL ) {
            int32_t result = 0;
            result = ACAMERA_FSM2CTX_PTR( p_fsm )->settings.lens_init( &p_fsm->lens_ctx, &p_fsm->lens_ctrl );
            if ( result != -1 && p_fsm->lens_ctx != NULL ) {
                const lens_param_t *p_lens_param = p_fsm->lens_ctrl.get_parameters( p_fsm->lens_ctx );
                p_fsm->lens_param = *p_lens_param;
                //only on lens init success populate param
                param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
                p_fsm->lens_driver_ok = 1;
            } else {
                p_fsm->lens_driver_ok = 0;
                return;
            }
        } else {
            p_fsm->lens_driver_ok = 0;
            return;
        }
#endif
    }

    if ( param ) {
        p_fsm->pos_min = p_fsm->def_pos_min = param->pos_min;
        p_fsm->pos_inf = p_fsm->def_pos_inf = param->pos_inf;
        p_fsm->pos_macro = p_fsm->def_pos_macro = param->pos_macro;
        p_fsm->pos_max = p_fsm->def_pos_max = param->pos_max;
        p_fsm->def_pos_min_down = param->pos_min_down;
        p_fsm->def_pos_inf_down = param->pos_inf_down;
        p_fsm->def_pos_macro_down = param->pos_macro_down;
        p_fsm->def_pos_max_down = param->pos_max_down;
        p_fsm->def_pos_min_up = param->pos_min_up;
        p_fsm->def_pos_inf_up = param->pos_inf_up;
        p_fsm->def_pos_macro_up = param->pos_macro_up;
        p_fsm->def_pos_max_up = param->pos_max_up;

        p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_START ) | ACAMERA_IRQ_MASK( ACAMERA_IRQ_AF2_STATS );
        AF_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );
    }
}
//================================================================================
void AF_request_irq( AF_fsm_ptr_t p_fsm )
{
    AF_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_AF2_STATS ) );
}
//================================================================================
void AF_init_fast_search( AF_fsm_ptr_t p_fsm )
{
    int16_t accel_angle = 0;

    if ( p_fsm->lens_driver_ok == 0 ) {
        LOG( LOG_ERR, "Lens driver was not initialized. AF will be stopped" );
        return;
    }

#if defined( FSM_PARAM_GET_ACCEL_DATA )
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_ACCEL_DATA, NULL, 0, &accel_angle, sizeof( accel_angle ) );
#endif

    if ( accel_angle ) {
        if ( accel_angle > 0 ) { // Facing up
            p_fsm->pos_min = p_fsm->def_pos_min + ( ( p_fsm->def_pos_min_up - p_fsm->def_pos_min ) * accel_angle >> 8 );
            p_fsm->pos_inf = p_fsm->def_pos_inf + ( ( p_fsm->def_pos_inf_up - p_fsm->def_pos_inf ) * accel_angle >> 8 );
            p_fsm->pos_macro = p_fsm->def_pos_macro + ( ( p_fsm->def_pos_macro - p_fsm->def_pos_macro ) * accel_angle >> 8 );
            p_fsm->pos_max = p_fsm->def_pos_max + ( ( p_fsm->def_pos_max_up - p_fsm->def_pos_max ) * accel_angle >> 8 );
        } else { // Facing down
            p_fsm->pos_min = p_fsm->def_pos_min + ( ( p_fsm->def_pos_min - p_fsm->def_pos_min_down ) * accel_angle >> 8 );
            p_fsm->pos_inf = p_fsm->def_pos_inf + ( ( p_fsm->def_pos_inf - p_fsm->def_pos_inf_down ) * accel_angle >> 8 );
            p_fsm->pos_macro = p_fsm->def_pos_macro + ( ( p_fsm->def_pos_macro - p_fsm->def_pos_macro_down ) * accel_angle >> 8 );
            p_fsm->pos_max = p_fsm->def_pos_max + ( ( p_fsm->def_pos_max - p_fsm->def_pos_max_down ) * accel_angle >> 8 );
        }

#ifdef TRACE
        printf( "Acceleromater data  %d\n", accel_angle );
        printf( "Defaul: min %d, inf %d macro %d, max %d, Recalculated min %d, inf %d, macro %d, max %d\n",
                p_fsm->def_pos_min, p_fsm->def_pos_inf, p_fsm->def_pos_macro, p_fsm->def_pos_max,
                p_fsm->pos_min, p_fsm->pos_inf, p_fsm->pos_macro, p_fsm->pos_max );
#endif
    }

    if ( p_fsm->mode == AF_MODE_MANUAL ) {
        uint16_t pos = p_fsm->pos_min + ( uint16_t )( ( uint32_t )( p_fsm->pos_max - p_fsm->pos_min ) * p_fsm->pos_manual >> 8 );

        if ( p_fsm->lens_ctrl.move ) {
            p_fsm->lens_ctrl.move( p_fsm->lens_ctx, pos );
        }

        p_fsm->last_position = pos;
        fsm_raise_event( p_fsm, event_id_af_converged );
#ifdef TRACE
        {
            const lens_param_t *lens_param = &p_fsm->lens_param;
            printf( "Set Manual Position to %d\n", pos / lens_param->min_step );
            //printf("Set Manual Position to %d\n", pos/p_fsm->vcm_drv.min_step);
        }
#endif
    } else {
        af_fast_search_param_t *fs = &p_fsm->fs;
        af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
        uint16_t *ptr_af_zone_whgh_h = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_ZONE_WGHT_HOR );
        uint16_t *ptr_af_zone_whgh_v = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_ZONE_WGHT_VER );

        uint8_t x, y;
        uint8_t zones_horiz = acamera_isp_metering_af_nodes_used_horiz_read( p_fsm->cmn.isp_base );
        uint8_t zones_vert = acamera_isp_metering_af_nodes_used_vert_read( p_fsm->cmn.isp_base );

        if ( !zones_horiz || !zones_vert ) {
            zones_horiz = ISP_DEFAULT_AF_ZONES_HOR;
            zones_vert = ISP_DEFAULT_AF_ZONES_VERT;
        }

        uint8_t x_start = ( uint8_t )( ( ( ( p_fsm->roi >> 24 ) & 0xFF ) * zones_horiz + 128 ) >> 8 );
        uint8_t x_end = ( uint8_t )( ( ( ( p_fsm->roi >> 8 ) & 0xFF ) * zones_horiz + 128 ) >> 8 );
        uint8_t y_start = ( uint8_t )( ( ( ( p_fsm->roi >> 16 ) & 0xFF ) * zones_vert + 128 ) >> 8 );
        uint8_t y_end = ( uint8_t )( ( ( ( p_fsm->roi >> 0 ) & 0xFF ) * zones_vert + 128 ) >> 8 );
#ifdef TRACE
        printf( "ROI: X from %d to %d out of %d, Y from %d to %d out of %d\n", x_start, x_end, zones_horiz, y_start, y_end, zones_vert );
#endif
        uint8_t zone_size_x = x_end - x_start;
        uint8_t zone_size_y = y_end - y_start;
        uint32_t middle_x = zone_size_x * 256 / 2;
        uint32_t middle_y = zone_size_y * 256 / 2;
        uint16_t gaus_center_x = ( _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_ZONE_WGHT_HOR ) * 256 / 2 );
        uint16_t gaus_center_y = ( _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_ZONE_WGHT_VER ) * 256 / 2 );

        for ( y = 0; y < zones_vert; y++ ) {
            uint32_t inx = (uint32_t)y * zones_horiz;
            for ( x = 0; x < zones_horiz; x++ ) {
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
                        p_fsm->zone_weight[inx + x] = 1;
                    } else {
                        coeff_x = ( gaus_center_x + distance_x ) / 256;
                        if ( distance_x > 0 && ( distance_x & 0x80 ) )
                            coeff_x--;
                        coeff_y = ( gaus_center_y + distance_y ) / 256;
                        if ( distance_y > 0 && ( distance_y & 0x80 ) )
                            coeff_y--;

                        coeff_x = ptr_af_zone_whgh_h[coeff_x];
                        coeff_y = ptr_af_zone_whgh_v[coeff_y];

                        p_fsm->zone_weight[inx + x] = ( coeff_x * coeff_y ) >> 4;
                        if ( !p_fsm->zone_weight[inx + x] )
                            p_fsm->zone_weight[inx + x] = 1;
                    }
                } else {
                    p_fsm->zone_weight[inx + x] = 1;
                }
            }
        }

        fs->step = 0;
        if ( p_fsm->mode == AF_MODE_CALIBRATION ) {
            const lens_param_t *lens_param = &p_fsm->lens_param;
            fs->position = AF_CALIBRATION_BOUNDARIES * lens_param->min_step;
            fs->step_number = ( uint16_t )( 0x10000 / lens_param->min_step - 2 * AF_CALIBRATION_BOUNDARIES + 1 );
        } else {
            fs->position = p_fsm->pos_min;
            fs->step_number = param->fast_search_positions;
        }
        fs->prev_position = fs->position;
        fs->spot_zone_step_x = ISP_DEFAULT_AF_ZONES_HOR / AF_SPOT_COUNT_X;
        fs->spot_zone_step_y = ISP_DEFAULT_AF_ZONES_VERT / AF_SPOT_COUNT_Y;
        fs->spot_zone_size_x = fs->spot_zone_step_x;
        fs->spot_zone_size_y = fs->spot_zone_step_y;
        fs->finished_valid_spot_count = 0;

#if defined( TRACE ) || defined( TRACE_IQ )
        printf( "Zone X size %d, step %d, Y size %d, step %d\n", fs->spot_zone_size_x, fs->spot_zone_step_x, fs->spot_zone_size_y, fs->spot_zone_step_y );
#endif
        for ( y = 0; y < AF_SPOT_COUNT_Y; y++ ) {
            for ( x = 0; x < AF_SPOT_COUNT_X; x++ ) {
                fs->spots[y][x].min_value = 0x7FFFFFFF;
                fs->spots[y][x].max_value = 0x80000000;
                fs->spots[y][x].max_position = p_fsm->pos_inf;
                fs->spots[y][x].before_max_value = 0x80000000;
                fs->spots[y][x].before_max_position = p_fsm->pos_inf;
                fs->spots[y][x].after_max_value = 0x80000000;
                fs->spots[y][x].after_max_position = p_fsm->pos_inf;
                fs->spots[y][x].previous_value = 0x80000000;
                fs->spots[y][x].status = 0;
                fs->spots[y][x].optimal_position = p_fsm->pos_inf;
                fs->spots[y][x].dynamic_range = 1024; // max dynamic range is 1024
            }
        }
        p_fsm->skip_frame = param->skip_frames_init; // first edge is big jump in position, so give it more time for stability

        if ( p_fsm->lens_ctrl.move ) {
            p_fsm->lens_ctrl.move( p_fsm->lens_ctx, fs->position );
        }

        p_fsm->last_position = fs->position;
        p_fsm->frame_number_from_start = 0;
#if defined( TRACE ) || defined( TRACE_IQ )
        printf( "Fast Search Started\n" );
#endif
    }
}
//================================================================================
static uint16_t calculate_position( uint16_t x1, uint16_t x2, uint16_t x3, int32_t y1, int32_t y2, int32_t y3 )
{
    int64_t nominator, denominator;
    nominator = (int64_t)y1 * ( ( int32_t )( (uint32_t)x2 * x2 - (uint32_t)x3 * x3 ) );
    nominator += (int64_t)y2 * ( ( int32_t )( (uint32_t)x3 * x3 - (uint32_t)x1 * x1 ) );
    nominator += (int64_t)y3 * ( ( int32_t )( (uint32_t)x1 * x1 - (uint32_t)x2 * x2 ) );
    denominator = (int64_t)y1 * ( (int32_t)x2 - (int32_t)x3 );
    denominator += (int64_t)y2 * ( (int32_t)x3 - (int32_t)x1 );
    denominator += (int64_t)y3 * ( (int32_t)x1 - (int32_t)x2 );
    if ( denominator )
        return ( uint16_t )( nominator / denominator / 2 );
    else
        return x2;
}

//================================================================================
static uint8_t is_frame_need_to_skip( AF_fsm_ptr_t p_fsm )
{
// If USER_MODULE is configured, AF is running in user-fw, need to skip frames which after lens movement in kern-FW,
// we added this new function because in user-FW, there is a queue for AF stats, after algorithm calculated new lens
// position, we need to ignore the AF stats which already in the queue, or else, if we tream them as the stats after
// lens movement, we will skip wrong stats and caused AF to start next step calculation based on wrong stats.
#if USER_MODULE
    uint8_t skip = 0;

    if ( p_fsm->skip_frame ) {
        skip = 1;

        // only decrease skip_frame when we process the frames after lens move which indicated by skip_cur_frame.
        if ( p_fsm->skip_cur_frame )
            p_fsm->skip_frame--;
    }

    LOG( LOG_INFO, "skip_cur_frame: %d, remain skip_frame: %d, return skip: %d.", p_fsm->skip_cur_frame, p_fsm->skip_frame, skip );

    return skip;
#else
    uint8_t skip = 0;

    if ( ( p_fsm->lens_ctrl.is_moving != NULL ) && p_fsm->lens_ctrl.is_moving( p_fsm->lens_ctx ) ) {
        // skip frames when lens is moving.
        skip = 1;
    } else if ( p_fsm->skip_frame ) {
        // p_fsm->skip_frame is the number of frames need to be skipped
        // after lens movement started.
        skip = 1;
        p_fsm->skip_frame--;
    }

    return skip;
#endif
}

void AF_process_fast_search_step( AF_fsm_ptr_t p_fsm )
{
    af_fast_search_param_t *fs = &p_fsm->fs;
    af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
    uint8_t zones_horiz = acamera_isp_metering_af_nodes_used_horiz_read( p_fsm->cmn.isp_base );
    uint8_t x, y;
    uint16_t i, j;

    if ( !zones_horiz ) {
        zones_horiz = ISP_DEFAULT_AF_ZONES_HOR;
    }

    int32_t middle_spot = 0;

    const lens_param_t *lens_param = &p_fsm->lens_param;

    if ( !p_fsm->frame_num ) {
        if ( !is_frame_need_to_skip( p_fsm ) ) {
            int32_t stat = AF_process_statistic( p_fsm );
            uint16_t pos = fs->position;

            uint8_t increasing = 0;
            uint8_t rising = 0;
            uint8_t falling = 0;

            for ( y = AF_SPOT_IGNORE_NUM; y < AF_SPOT_COUNT_Y - AF_SPOT_IGNORE_NUM; y++ ) {
                uint16_t y_off = fs->spot_zone_step_y * y;
                for ( x = AF_SPOT_IGNORE_NUM; x < AF_SPOT_COUNT_X - AF_SPOT_IGNORE_NUM; x++ ) {
                    uint16_t x_off = fs->spot_zone_step_x * x;
                    int32_t spot_stat = 0;
                    uint32_t spot_reliable = 0;
                    af_spot_param_t *spot = &fs->spots[y][x];
                    if ( AF_SPOT_STATUS_NOT_FINISHED == spot->status ) {
                        uint64_t spot_acc = 0;
                        for ( j = y_off; j < y_off + fs->spot_zone_size_y; j++ ) {
                            uint16_t inx = j * zones_horiz;
                            for ( i = x_off; i < x_off + fs->spot_zone_size_x; i++ ) {
                                spot_acc += p_fsm->zone_process_statistic[inx + i];
                                spot_reliable += p_fsm->zone_process_reliablility[inx + i];
                            }
                        }
                        spot_stat = acamera_log2_fixed_to_fixed_64( spot_acc, 0, LOG2_GAIN_SHIFT );

                        if ( x == AF_SPOT_COUNT_X / 2 && y == AF_SPOT_COUNT_Y / 2 ) {
                            middle_spot = spot_stat;
                        }

                        if ( spot_stat > spot->previous_value ) {
                            rising++;
                        } else {
                            // for falling spot we use exit_threshold to check.
                            if ( ( spot->previous_value - spot_stat ) > param->exit_th )
                                falling++;
                        }

                        if ( spot_stat > spot->max_value ) {
                            spot->before_max_value = spot->previous_value;
                            spot->before_max_position = fs->prev_position;
                            spot->max_value = spot_stat;
                            spot->max_position = fs->position;
                            spot->after_max_value = 0x80000000;
                        } else if ( spot->max_position == fs->prev_position ) { // maximum was found on the previous step
                            spot->after_max_value = spot_stat;
                            spot->after_max_position = fs->position;
                        }
                        if ( spot_stat < spot->min_value ) {
                            spot->min_value = spot_stat;
                        }

                        if ( p_fsm->mode != AF_MODE_CALIBRATION &&
                             ( ( spot->max_value > spot_stat + param->exit_th ) || ( fs->step == fs->step_number + 1 ) ) ) {

                            spot->status = AF_SPOT_STATUS_FINISHED_VALID;
                            fs->finished_valid_spot_count++;

                            if ( spot->before_max_value == 0x80000000 || spot->after_max_value == 0x80000000 ) { // never increase or never decrease - set maximum position
                                spot->optimal_position = spot->max_position;
                            } else { // central point calculation
                                spot->optimal_position = calculate_position( spot->after_max_position, spot->max_position, spot->before_max_position, spot->after_max_value, spot->max_value, spot->before_max_value );
                            }

                            if ( spot->optimal_position < p_fsm->pos_inf ) {
                                spot->optimal_position = p_fsm->pos_inf;
                            } else if ( spot->optimal_position > p_fsm->pos_macro ) {
                                spot->optimal_position = p_fsm->pos_macro;
                            }
#ifdef TRACE
                            printf( "spot %dx%d finished with position %d, max_value %ld\n", x, y, spot->optimal_position / lens_param->min_step, spot->max_value );
#endif
                        } else {
                            increasing = 1;
                        }
                        spot->previous_value = spot_stat;
                        spot->reliable = spot_reliable;

                        if ( param->print_debug )
                            LOG( LOG_NOTICE, "spot %d x %d, dynamic_range: %u, stat: %d, reliable: %d, max: %d, min: %d.",
                                 x,
                                 y,
                                 spot->dynamic_range,
                                 spot_stat,
                                 spot_reliable,
                                 spot->max_value,
                                 spot->min_value );
                    }
                }
            }

            // Only check invalid spot when we found at least 1 valid spot.
            if ( p_fsm->mode != AF_MODE_CALIBRATION && fs->finished_valid_spot_count ) {
                for ( y = AF_SPOT_IGNORE_NUM; y < AF_SPOT_COUNT_Y - AF_SPOT_IGNORE_NUM; y++ ) {
                    for ( x = AF_SPOT_IGNORE_NUM; x < AF_SPOT_COUNT_X - AF_SPOT_IGNORE_NUM; x++ ) {
                        af_spot_param_t *spot = &fs->spots[y][x];

                        // start calc dynamic range from step 3.
                        if ( fs->step >= 2 ) {
                            if ( spot->max_value != 0 ) {
                                spot->dynamic_range = ( ( spot->max_value - spot->min_value ) << 10 ) / spot->max_value;
                            } else {
                                LOG( LOG_ERR, "spot->max_value is 0, avoided divide by 0. " );
                            }

                            if ( spot->dynamic_range < param->dynamic_range_th ) {
                                // mark low contrast spot as invalid spot
                                spot->status = AF_SPOT_STATUS_FINISHED_INVALID;
                            }
                        }
                    }
                }
            }

            if ( p_fsm->mode == AF_MODE_CALIBRATION ) {
                p_fsm->last_sharp = middle_spot;
#ifdef TRACE
                printf( "%d,%ld\n", pos / lens_param->min_step, middle_spot );
#endif
            }

            if ( rising >= falling ) {
                increasing = 1;
            } else {
                increasing = 0;
            }

            if ( param->print_debug )
                LOG( LOG_NOTICE, "increasing: %d (^%d vs v%d), finished_valid_spot_count: %d, step: %d, step_number: %d.", increasing, rising, falling, fs->finished_valid_spot_count, fs->step, fs->step_number );

            p_fsm->frame_number_from_start++;
            if ( ( p_fsm->mode != AF_MODE_CALIBRATION && !increasing ) || fs->step == fs->step_number + 1 ) {
                uint16_t best_position = 0;
                uint32_t best_reliable = 0xFFFFFF00;

                if ( param->print_debug )
                    LOG( LOG_NOTICE, "Final position calc." );

                for ( y = AF_SPOT_IGNORE_NUM; y < AF_SPOT_COUNT_Y - AF_SPOT_IGNORE_NUM; y++ ) {
                    for ( x = AF_SPOT_IGNORE_NUM; x < AF_SPOT_COUNT_X - AF_SPOT_IGNORE_NUM; x++ ) {
                        af_spot_param_t *spot = &fs->spots[y][x];

                        if ( x != AF_SPOT_COUNT_X / 2 && y != AF_SPOT_COUNT_Y / 2 )
                            continue;

                        if ( spot->status != AF_SPOT_STATUS_FINISHED_VALID )
                            continue;

                        if ( ( spot->max_value - spot->min_value ) > ( param->spot_tolerance ) ) {
                            if ( best_position < spot->optimal_position ) {
                                best_position = spot->optimal_position;
                                best_reliable = MIN( spot->reliable, best_reliable );
#ifdef TRACE
                                printf( "Best position: %d, for spot: %dx%d, reliable %ld\n", best_position / lens_param->min_step, x, y, spot->reliable );
#endif
                                if ( param->print_debug )
                                    LOG( LOG_NOTICE, "Best position: %d, for spot: %dx%d, reliable %u.", best_position / lens_param->min_step, x, y, spot->reliable );
                            }

                        }
#ifdef TRACE
                        else {
                            printf( "Skip low contrast spot %dx%d, diff %ld, reliable %ld\n", x, y, spot->max_value - spot->min_value, spot->reliable );
                        }
#endif
                    }
                }
                if ( !best_position ) { // all spots are low contrast. Choose middle one.
                    best_position = fs->spots[AF_SPOT_COUNT_Y / 2][AF_SPOT_COUNT_X / 2].optimal_position;
#ifdef TRACE
                    printf( "All spot are low contrast. Choose position from central spot: %d\n", best_position / lens_param->min_step );
#endif
                    if ( param->print_debug )
                        LOG( LOG_NOTICE, "All spot are low contrast. Choose position from central spot: %d.", best_position / lens_param->min_step );
                }

                fs->position = best_position;

#ifdef TRACE
                printf( "Position: %d, value: %d\n", pos / lens_param->min_step, stat );
#endif
                if ( param->print_debug )
                    LOG( LOG_NOTICE, "Position: %d, value: %d.", pos / lens_param->min_step, stat );

                if ( p_fsm->lens_ctrl.move ) {
                    p_fsm->lens_ctrl.move( p_fsm->lens_ctx, fs->position );
                }

                p_fsm->last_position = fs->position;
                if ( param->print_debug )
                    LOG( LOG_NOTICE, "fast_search_finished, last_position: %d.", p_fsm->last_position >> 6 );
                fsm_raise_event( p_fsm, event_id_af_fast_search_finished );
#ifdef TRACE
                printf( "Found optimal position: %d\n", fs->position / lens_param->min_step );
#endif
                if ( param->print_debug ) {
                    LOG( LOG_NOTICE, "### N: %d FS P: %d.", p_fsm->frame_number_from_start, fs->position >> 6 );
                }

#ifdef TRACE_IQ
                printf( "### N: %ld FS P: %d\n", p_fsm->frame_number_from_start, fs->position );
#endif
                return;
            } else {
                if ( param->print_debug )
                    LOG( LOG_NOTICE, "Move to next step." );

                fs->step++;
                fs->prev_position = fs->position;
                if ( p_fsm->mode == AF_MODE_CALIBRATION ) {
                    fs->position = ( AF_CALIBRATION_BOUNDARIES + fs->step ) * lens_param->min_step;
                } else {
                    if ( fs->step <= fs->step_number ) {
                        fs->position = p_fsm->pos_inf + ( uint16_t )( ( uint32_t )( p_fsm->pos_macro - p_fsm->pos_inf ) * ( fs->step - 1 ) / ( fs->step_number - 1 ) );
                    } else {
                        fs->position = param->pos_max;
                    }
                }
                if ( p_fsm->mode == AF_MODE_CALIBRATION ) {
                    // position is already set in frame end IRQ handler
                    if ( p_fsm->lens_ctrl.move ) {
                        p_fsm->lens_ctrl.move( p_fsm->lens_ctx, fs->position );
                    }
                    p_fsm->last_position = fs->position;
                    if ( param->print_debug )
                        LOG( LOG_NOTICE, "calibration_mode, last_position: %d.", p_fsm->last_position >> 6 );
                    p_fsm->skip_frame = param->skip_frames_move; // skip next frames
                } else {
                    // For user space implementation. If algorithm is in kernel the lens movement should have been already done by
                    // interrupt handler. It should be safe to move the lens twice to the same position
                    {
                        af_fast_search_param_t *fs = &p_fsm->fs;
                        uint8_t step = fs->step;
                        uint16_t position;
                        if ( step <= fs->step_number ) {
                            position = p_fsm->pos_inf + ( uint16_t )( ( uint32_t )( p_fsm->pos_macro - p_fsm->pos_inf ) * ( step - 1 ) / ( fs->step_number - 1 ) );
                        } else {
                            position = p_fsm->pos_max;
                        }

                        if ( p_fsm->lens_ctrl.move ) {
                            p_fsm->lens_ctrl.move( p_fsm->lens_ctx, position );
                        }
                        p_fsm->last_position = position;
                        if ( param->print_debug )
                            LOG( LOG_NOTICE, "last_position: %d, lens: %d.", p_fsm->last_position, p_fsm->last_position >> 6 );
                    }
                    p_fsm->skip_frame = param->skip_frames_move; // skip next frames
                }
#ifdef TRACE
                printf( "Position: %d, value: %ld\n", pos / lens_param->min_step, stat );
#endif
            }

            if ( param->print_debug )
                LOG( LOG_NOTICE, "### N: %d FS P: %d, lens: %d.", p_fsm->frame_number_from_start, fs->position, fs->position >> 6 );
#ifdef TRACE_IQ
            printf( "### N: %ld FS P: %d\n", p_fsm->frame_number_from_start, fs->position );
#endif
        }
    }

    status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATUS_INFO );
    p_status_info->af_info.lens_pos = p_fsm->last_position;
    p_status_info->af_info.focus_value = p_fsm->last_sharp;

    fsm_raise_event( p_fsm, event_id_af_step );
}
//================================================================================
void AF_init_track_position( AF_fsm_ptr_t p_fsm )
{
    af_fast_search_param_t *fs = &p_fsm->fs;
    af_track_position_param_t *tp = &p_fsm->tp;
    af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
    p_fsm->skip_frame = param->skip_frames_init;

    system_memset( fs, 0, sizeof( af_fast_search_param_t ) );
    tp->frames_in_tracking = array_size( tp->values ) + 2;
    tp->scene_is_changed = 0;

#ifdef TRACE
    printf( "Position Track Started from position %d\n", fs->position );
#endif
}
//================================================================================


void AF_process_track_position_step( AF_fsm_ptr_t p_fsm )
{
    af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );

    int32_t threshold_low = param->caf_stable_th;
    int32_t threshold_high = param->caf_trigger_th;

    if ( p_fsm->mode != AF_MODE_CAF ) {
#ifdef TRACE
        printf( "AF has converged\n" );
#endif
        fsm_raise_event( p_fsm, event_id_af_converged );
        return;
    }

    if ( ( !p_fsm->lens_ctrl.is_moving || !p_fsm->lens_ctrl.is_moving( p_fsm->lens_ctx ) ) && !--p_fsm->skip_frame ) { // skip this frame or frame counter
        af_track_position_param_t *tp = &p_fsm->tp;
        if ( ++tp->values_inx >= array_size( tp->values ) )
            tp->values_inx = 0;
        tp->values[tp->values_inx] = AF_process_statistic( p_fsm );
        p_fsm->skip_frame = 1;

        if ( tp->frames_in_tracking )
            tp->frames_in_tracking--;
        else {
            static uint32_t diff0_cnt = 0;
            int i;
            int32_t stat_min = 0x7FFFFFFF;
            int32_t stat_max = -0x7FFFFFFF;
            for ( i = 0; i < array_size( tp->values ); i++ ) {
                if ( stat_min > tp->values[i] )
                    stat_min = tp->values[i];
                if ( stat_max < tp->values[i] )
                    stat_max = tp->values[i];
            }

            if ( param->print_debug ) {
                if ( 0 == ( ( stat_max - stat_min ) >> ( LOG2_GAIN_SHIFT - 4 ) ) ) {
                    diff0_cnt++;
                } else {
                    LOG( LOG_INFO, "diff0_cnt: %d, low: %d, high: %d, min %d max %d diff %d.",
                         diff0_cnt,
                         threshold_low >> ( LOG2_GAIN_SHIFT - 4 ),
                         threshold_high >> ( LOG2_GAIN_SHIFT - 4 ),
                         stat_min >> ( LOG2_GAIN_SHIFT - 4 ),
                         stat_max >> ( LOG2_GAIN_SHIFT - 4 ),
                         ( stat_max - stat_min ) >> ( LOG2_GAIN_SHIFT - 4 ) );

                    diff0_cnt = 0;
                }
            }

#if defined( TRACE )
            printf( "min %ld max %ld diff %ld\n", stat_min, stat_max, ( stat_max - stat_min ) >> ( LOG2_GAIN_SHIFT - 4 ) );
#endif
            if ( !tp->scene_is_changed ) {
                if ( stat_max - stat_min > threshold_high ) {
                    tp->scene_is_changed = 1;

                    if ( param->print_debug ) {
                        LOG( LOG_NOTICE, "Scene is changed - wait for stabilization." );
                    }

#if defined( TRACE_SCENE_CHANGE )
                    printf( "Scene is changed - wait for stabilization\n" );
#endif
                }
            } else if ( stat_max - stat_min < threshold_low ) {
                if ( param->print_debug ) {
                    LOG( LOG_NOTICE, "Scene is stable - full refocus." );
                }

#if defined( TRACE_SCENE_CHANGE )
                printf( "Scene is stable - full refocus\n" );
#endif
                fsm_raise_event( p_fsm, event_id_af_refocus );
                return;
            }
        }
    }

    fsm_raise_event( p_fsm, event_id_af_step );
}
//================================================================================

void AF_deinit( AF_fsm_ptr_t p_fsm )
{
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.lens_deinit )
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->settings.lens_deinit( p_fsm->lens_ctx );
}
