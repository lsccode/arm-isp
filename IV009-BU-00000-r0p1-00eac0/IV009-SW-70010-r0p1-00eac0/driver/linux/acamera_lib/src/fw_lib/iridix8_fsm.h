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

#if !defined( __IRIDIX_FSM_H__ )
#define __IRIDIX_FSM_H__



typedef struct _iridix_fsm_t iridix_fsm_t;
typedef struct _iridix_fsm_t *iridix_fsm_ptr_t;
typedef const struct _iridix_fsm_t *iridix_fsm_const_ptr_t;

void iridix_initialize( iridix_fsm_ptr_t p_fsm );
void iridix_update( iridix_fsm_ptr_t p_fsm );


struct _iridix_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint16_t strength_target;
    uint32_t strength_avg;
    uint32_t mode;
    uint16_t dark_enh;
    uint32_t dark_enh_avg;
    uint32_t iridix_global_DG_avg;
    uint16_t mp_iridix_strength;
    uint32_t iridix_contrast;
    uint16_t iridix_global_DG;
    uint16_t iridix_global_DG_prev;

    uint32_t frame_id_tracking;
};


void iridix_fsm_clear( iridix_fsm_ptr_t p_fsm );

void iridix_fsm_init( void *fsm, fsm_init_param_t *init_param );
int iridix_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int iridix_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t iridix_fsm_process_event( iridix_fsm_ptr_t p_fsm, event_id_t event_id );

void iridix_fsm_process_interrupt( iridix_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void iridix_request_interrupt( iridix_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __IRIDIX_FSM_H__ */
