//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT 2018 ARM Limited or its affiliates
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#ifndef __I2C_FTDI_H__
#define __I2C_FTDI_H__

#include <vector>
#include <ARMCPP11/Atomic.h>
#include <ARMCPP11/Chrono.h>
#include <ARMCPP11/Thread.h>
#include <ARMCPP11/Mutex.h>
#include <deque>
#include <ARMCPP11/ConditionVariable.h>

#include <ATL/ATLObject.h>
#include <ATL/ATLComponent.h>

#include <BusManager/BaseDriverInterface.h>

#include "FTDIDriver.h"

namespace act {

    class CI2CDriver : public virtual CBaseDriver, public virtual CATLObject {
    protected:
        // list of connected devices
        std::vector< std::string > devices;
    public:
        CI2CDriver() : ftdi_opened(false), bitbang(false), led_status(false) {}
        ~CI2CDriver() {if (rx_thread.Joinable()) Close();}

        // IATLObject interface
        inline virtual const std::string GetObjectStaticName() {return "CI2CDriver";}

        // IATLComponent
        inline virtual const std::string GetName() {return "i2c";}
        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        virtual CATLError ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32 & options = 0);
        virtual const std::vector<std::string > GetDeviceList();
        virtual const std::vector<EPacketCommand> GetCommandList();
        virtual const EACTDriverMode GetCurrentMode();
        virtual const std::vector<EACTDriverMode> GetSupportedModes();

    private:
        static const UInt32 page_bits = 7;
        static const UInt32 page_size = 1 << page_bits;
        static const UInt32 page_mask = page_size-1;
        static const UInt8 dev_address = 0x1C;
        static const UInt8 mpsse_dir;
        static const int bitbang_delay = 2;
        static const int scl_phase_num = 3;
        static const basesize maximum_tx_queue = 100;

        std::string dev_name;
        FTDI ftdi;
        bool ftdi_opened;
        bool bitbang;
        UInt32 baudrate;
        UInt32 maximum_bytes_per_transaction;
        CATLError read_data (TSmartPtr<CTransferPacket>& packet);
        CATLError write_data(TSmartPtr<CTransferPacket>& packet);
        void configure();

        // TX part
        struct RxMessage : public CATLObject {
            bool read_modify_write;
            basesize size;
            std::vector<basesize> ack_position;
            std::vector<basesize> byte_position;
            TSmartPtr<CTransferPacket> packet;

            RxMessage(TSmartPtr<CTransferPacket>& pack, bool rmw) : read_modify_write(rmw), size(0), packet(pack) {}
            const std::string GetObjectStaticName() {return "RxMessage";}
        };
        void create_threads(void);
        void stop_threads(void);
        armcpp11::Atomic<bool> threads_stop;

        // TX part
        void tx_thread_processing();
        static void* tx_thread_wrapper(void *arg);
        armcpp11::Thread tx_thread;
        armcpp11::Mutex tx_mutex; // mutex locks all variables below
        armcpp11::ConditionVariable tx_cv;
        std::deque<TSmartPtr<CTransferPacket> > tx_queue;
        std::deque<TSmartPtr<CTransferPacket> > tx_queue_write;
        int current_page;
        std::vector<UInt8> tx_buffer;
        basesize tx_buffer_inx;
        void set_page(RxMessage& msg, UInt32 addr);
        void prepare_data_write(RxMessage& msg);
        void prepare_data_read(RxMessage& msg);
        void put_chunk_write(RxMessage& msg, UInt8 addr, const UInt8* data, UInt32 size, bool last = false);
        void put_chunk_read(RxMessage& msg, UInt32 size, bool last = false);
        void put_tx_packet(TSmartPtr<CTransferPacket>& packet, bool write = false);
        TSmartPtr<RxMessage> prepare_data(TSmartPtr<CTransferPacket>& packet, bool check_mask = true);

        // RX part
        void rx_thread_processing();
        static void* rx_thread_wrapper(void *arg);
        armcpp11::Thread rx_thread;
        armcpp11::Mutex rx_mutex; // mutex locks all variables below
        armcpp11::ConditionVariable rx_cv;
        std::deque<std::deque<TSmartPtr<RxMessage> > > rx_queues;
        std::vector<UInt8> rx_buffer;
        void parse_rec_data(RxMessage& msg, const UInt8* buf, basesize size);
        void put_rx_messages(std::deque<TSmartPtr<RxMessage> >& msg);
        void rx_data_read(std::deque<TSmartPtr<RxMessage> >& msg_queue);
        bool is_rx_queue_empty();

        // I2C layer functions
        void put_start();
        void put_stop();
        void put_cont_start();
        void put_byte(RxMessage& msg, UInt8 byte);
        void put_bit(UInt8 bit);
        void get_bit(RxMessage& msg);
        void get_byte(RxMessage& msg, bool last = false);

        // bitbang
        UInt8 bitbang_scl;
        UInt8 bitbang_sda_in;
        UInt8 bitbang_sda_out;

        // MPSSE
        void mpsse_out(bool scl, bool sda);
        void mpsse_led(bool on);
        bool led_status;
    };
}
#endif /* __I2C_FTDI_H__ */
