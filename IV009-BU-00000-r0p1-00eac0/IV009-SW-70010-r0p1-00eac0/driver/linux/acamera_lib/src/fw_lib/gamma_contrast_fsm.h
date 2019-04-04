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

#if !defined( __GAMMA_CONTRAST_FSM_H__ )
#define __GAMMA_CONTRAST_FSM_H__



typedef struct _gamma_contrast_fsm_t gamma_contrast_fsm_t;
typedef struct _gamma_contrast_fsm_t *gamma_contrast_fsm_ptr_t;
typedef const struct _gamma_contrast_fsm_t *gamma_contrast_fsm_const_ptr_t;

void gamma_contrast_init( gamma_contrast_fsm_ptr_t p_fsm );
void gamma_contrast_process( gamma_contrast_fsm_ptr_t p_fsm );
void gamma_contrast_read_histogram( gamma_contrast_fsm_ptr_t p_fsm );


struct _gamma_contrast_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    int32_t wb_log2[4];
    uint32_t fullhist_sum;
    uint32_t fullhist[ISP_FULL_HISTOGRAM_SIZE];
    uint32_t cumu_hist[ISP_FULL_HISTOGRAM_SIZE];
    uint16_t gain_target;
    uint32_t gain_avg;
    uint16_t nodes_target;
    uint32_t nodes_avg;

    uint32_t cur_using_stats_frame_id;
    uint32_t gamma_gain;
    uint32_t gamma_offset;
};


void gamma_contrast_fsm_clear( gamma_contrast_fsm_ptr_t p_fsm );

void gamma_contrast_fsm_init( void *fsm, fsm_init_param_t *init_param );
int gamma_contrast_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int gamma_contrast_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t gamma_contrast_fsm_process_event( gamma_contrast_fsm_ptr_t p_fsm, event_id_t event_id );

void gamma_contrast_fsm_process_interrupt( gamma_contrast_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void gamma_contrast_request_interrupt( gamma_contrast_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __GAMMA_CONTRAST_FSM_H__ */
