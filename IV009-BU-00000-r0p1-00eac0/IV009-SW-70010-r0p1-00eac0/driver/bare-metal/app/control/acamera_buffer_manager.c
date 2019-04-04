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
#include "acamera_math.h"
#include "acamera_logger.h"
#include "acamera_connection.h"
#include "acamera_buffer_manager.h"
#include "acamera_cmd_queues_config.h"

enum {
    FW_IS_RESET = 0,
    CHANNEL_REQUESTED,
    CHANNEL_ACKNOWLEDGE,
    WRITE_REGISTER_UPDATED,
    READ_REGISTER_UPDATED
};

static const uint32_t state_offset = 12;     // offset of state register in the memory
static const uint32_t write_inx_offset = 16; // offset of write index register in the memory
static const uint32_t read_inx_offset = 20;  // offset of read index register in the memory
static const uint32_t data_offset = 24;      // actual data offset in the memory

static void read_portion( uint32_t addr, uint32_t *data, uint32_t size )
{
    while ( size >= 4 ) {
        *data++ = system_hw_read_32( addr );
        addr += 4;
        size -= 4;
    }
}

static void write_portion( uint32_t addr, const uint32_t *data, uint32_t size )
{
    while ( size >= 4 ) {
        system_hw_write_32( addr, *data++ );
        addr += 4;
        size -= 4;
    }
}

static void set_value( uint32_t addr, uint32_t data )
{
    data = ( data & 0xFFFF ) | ( data << 16 );
    system_hw_write_32( addr, data );
}

#define ERROR_VALUE 0xFFFFFFFF

static uint32_t get_value( uint32_t addr, int check )
{
    uint32_t res = 0;
    if ( check ) {
        int cnt = 10; // do this only some time
        do {
            read_portion( addr, &res, sizeof( uint32_t ) );
        } while ( ( res & 0xFFFF ) != ( res >> 16 ) && --cnt );
        if ( cnt )
            return res & 0xFFFF;
        else
            return ERROR_VALUE;
    } else {
        read_portion( addr, &res, sizeof( uint32_t ) );
        return res & 0xFFFF;
    }
}

void acamera_buffer_manager_init( acamera_buffer_manager_t *p_ctrl, uint32_t ref_addr, uint32_t size )
{
    size >>= 1;
    p_ctrl->rx_base = ref_addr;
    p_ctrl->tx_base = ref_addr + size;
    p_ctrl->data_size = size - data_offset;
    set_value( p_ctrl->rx_base + state_offset, FW_IS_RESET ); // notify sender that we are reset
    set_value( p_ctrl->tx_base + state_offset, FW_IS_RESET );
    set_value( p_ctrl->rx_base + read_inx_offset, 0 );  // reset index
    set_value( p_ctrl->tx_base + write_inx_offset, 0 ); // reset index
    p_ctrl->rx_ack = 0;
    p_ctrl->tx_ack = 0;
    LOG( LOG_INFO, "FW bus channel is reset\n" );
}

int acamera_buffer_manager_read( acamera_buffer_manager_t *p_ctrl, uint8_t *data, int size )
{
    uint32_t wr_inx, rd_inx, available, read_size;
    uint32_t state = get_value( p_ctrl->rx_base + state_offset, 0 );
    switch ( state ) {
    case FW_IS_RESET:
    case CHANNEL_ACKNOWLEDGE:
    case ERROR_VALUE:
        return 0;
    case CHANNEL_REQUESTED:
        LOG( LOG_INFO, "Accepted channel request on RX part. Reset receiver\n" );
        set_value( p_ctrl->rx_base + read_inx_offset, 0 );                // reset index
        set_value( p_ctrl->rx_base + state_offset, CHANNEL_ACKNOWLEDGE ); // notify sender that we accept chanel
        p_ctrl->rx_ack = 1;
        return BUS_ERROR_RESET;
    case WRITE_REGISTER_UPDATED:
    case READ_REGISTER_UPDATED:
        if ( p_ctrl->rx_ack ) {
            wr_inx = get_value( p_ctrl->rx_base + write_inx_offset, 1 );
            rd_inx = get_value( p_ctrl->rx_base + read_inx_offset, 1 );
            if ( wr_inx == ERROR_VALUE || rd_inx == ERROR_VALUE )
                return 0;
            available = ( wr_inx >= rd_inx ) ? wr_inx - rd_inx : wr_inx + p_ctrl->data_size - rd_inx; // available data size
            if ( !available )
                return 0;
            if ( size > available )
                size = available;                                                                // limit requested size to avalable amount
            read_size = MIN( size, p_ctrl->data_size - rd_inx );                                 // calculate size of first portion of data
            read_portion( p_ctrl->rx_base + data_offset + rd_inx, (uint32_t *)data, read_size ); // read first portion of data
            rd_inx += read_size;                                                                 // update read index
            if ( rd_inx >= p_ctrl->data_size )
                rd_inx = 0; // buffer roll over
            if ( read_size < size ) {
                uint32_t second_read = size - read_size;                                                               // calculate size of second portion of data
                read_portion( p_ctrl->rx_base + data_offset + rd_inx, (uint32_t *)( data + read_size ), second_read ); // read second portion of data
                read_size += second_read;                                                                              // update read size
                rd_inx += second_read;                                                                                 // update read index
                if ( rd_inx >= p_ctrl->data_size )
                    rd_inx = 0; // buffer roll over
            }
            set_value( p_ctrl->rx_base + read_inx_offset, rd_inx );             // set new index
            set_value( p_ctrl->rx_base + state_offset, READ_REGISTER_UPDATED ); // set flag
            LOG( LOG_DEBUG, "Received portion of data size %ld\n", read_size );
            return read_size;
        } else {
            set_value( p_ctrl->rx_base + state_offset, FW_IS_RESET ); // notify sender that we are in reset
            LOG( LOG_WARNING, "Request on non-established channel\n" );
            return 0;
        }
    }
    LOG( LOG_ERR, "Wrong channel state %ld\n", state );
    p_ctrl->rx_ack = 0;
    return BUS_ERROR_FATAL;
}

int acamera_buffer_manager_write( acamera_buffer_manager_t *p_ctrl, const uint8_t *data, int size )
{
    uint32_t wr_inx, rd_inx, free_space, write_size;
    uint32_t state = get_value( p_ctrl->tx_base + state_offset, 0 );
    switch ( state ) {
    case FW_IS_RESET:
        return 0;
    case CHANNEL_ACKNOWLEDGE:
        state = get_value( p_ctrl->rx_base + state_offset, 0 );
        if ( state == CHANNEL_REQUESTED )
            return BUS_ERROR_RESET;
        return 0;
    case ERROR_VALUE:
        return 0;
    case CHANNEL_REQUESTED:
        LOG( LOG_INFO, "Accepted channel request on TX part. Reset tramsmitter\n" );
        set_value( p_ctrl->tx_base + write_inx_offset, 0 );               // reset index
        set_value( p_ctrl->tx_base + state_offset, CHANNEL_ACKNOWLEDGE ); // notify sender that we accept chanel
        p_ctrl->tx_ack = 1;                                               // first acknowlege, just return 0
        return 0;
    case WRITE_REGISTER_UPDATED:
    case READ_REGISTER_UPDATED:
        if ( p_ctrl->tx_ack ) {
            wr_inx = get_value( p_ctrl->tx_base + write_inx_offset, 1 );
            rd_inx = get_value( p_ctrl->tx_base + read_inx_offset, 1 );
            if ( wr_inx == ERROR_VALUE || rd_inx == ERROR_VALUE )
                return 0;
            free_space = ( wr_inx >= rd_inx ) ? rd_inx - 4 - wr_inx + p_ctrl->data_size : rd_inx - 4 - wr_inx; // free space calculation
            if ( !free_space )
                return 0;
            if ( size > free_space )
                size = free_space;                                                                 // limit requested size to avalable amount
            write_size = MIN( size, p_ctrl->data_size - wr_inx );                                  // calculate size of first portion of data
            write_portion( p_ctrl->tx_base + data_offset + wr_inx, (uint32_t *)data, write_size ); // write first portion of data
            wr_inx += write_size;                                                                  // update write index
            if ( wr_inx >= p_ctrl->data_size )
                wr_inx = 0; // buffer roll over
            if ( write_size < size ) {
                uint32_t second_write = size - write_size;                                                                // calculate size of second portion of data
                write_portion( p_ctrl->tx_base + data_offset + wr_inx, (uint32_t *)( data + write_size ), second_write ); // write second portion of data
                write_size += second_write;                                                                               // update write amount
                wr_inx += second_write;                                                                                   // update write index
                if ( wr_inx >= p_ctrl->data_size )
                    wr_inx = 0; // buffer roll over
            }
            set_value( p_ctrl->tx_base + write_inx_offset, wr_inx );             // set new index
            set_value( p_ctrl->tx_base + state_offset, WRITE_REGISTER_UPDATED ); // set flag
            LOG( LOG_DEBUG, "Written portion of data size %ld, new wr_inx %ld\n", write_size, wr_inx );
            return write_size;
        } else {
            set_value( p_ctrl->tx_base + state_offset, FW_IS_RESET ); // notify sender that we are in reset
            LOG( LOG_WARNING, "Request on non-established channel\n" );
            return 0;
        }
    }
    LOG( LOG_ERR, "Wrong channel state %ld\n", state );
    p_ctrl->tx_ack = 0;
    return BUS_ERROR_FATAL;
}
