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

#if !defined( __ACAMERA_BUFFER_MANAGER_H__ )
#define __ACAMERA_BUFFER_MANAGER_H__

typedef struct {
    uint32_t tx_base;
    uint32_t rx_base;
    uint32_t data_size;
    int rx_ack;
    int tx_ack;
} acamera_buffer_manager_t;

void acamera_buffer_manager_init( acamera_buffer_manager_t *p_ctrl, uint32_t base, uint32_t size );
int acamera_buffer_manager_read( acamera_buffer_manager_t *p_ctrl, uint8_t *data, int size );
int acamera_buffer_manager_write( acamera_buffer_manager_t *p_ctrl, const uint8_t *data, int size );

#endif /* __ACAMERA_BUFFER_MANAGER_H__ */
