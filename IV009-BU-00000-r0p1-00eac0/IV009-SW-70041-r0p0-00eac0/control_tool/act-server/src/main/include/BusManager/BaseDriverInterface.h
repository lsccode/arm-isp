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

#ifndef __BASE_DRIVER_INTERFACE__
#define __BASE_DRIVER_INTERFACE__

#include <ATL/ATLTypes.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>
#include <ATL/ATLComponent.h>

#include "BusManager/TransferPacket.h"

using namespace atl ;

#include <vector>

namespace act {

    typedef enum _EACTDriverMode {
        EACTDriverSyncMode = 0,
        EACTDriverASyncMode
    } EACTDriverMode;

    typedef enum _EACTDriverListenerTypes {
        EACTForwardReply = 0
    } EACTDriverListenerTypes;

    typedef enum _EACTDriverProcessPacketFlags {
        EACTDriverProcessPacketSync = 1
    } EACTDriverProcessPacketFlags;

    class IDriver : public virtual IATLComponent {
    public:

        /**
         * Process a packet of data through the driver
         *
         * This routine will write a data from an input packet
         * and return a result of operation in a callback.
         * @param packet - a packet to be processed by the driver
         * @param options - initialization flags
         *
         * @return EATLErrorOk - initialization succeed
         *         EATLErrorFatal - initialization has failed.
         */
        virtual CATLError ProcessPacket(TSmartPtr<CTransferPacket> packet, const flags32& options = 0) = 0;

        /**
         * Return a list of supported modes
         *
         * This routine will return a list of modes
         * which are supported by the driver: async or sync.
         * Please see EACTDriverMode enum
         *
         * @return - a list of driver modes
         */
        virtual const std::vector<EACTDriverMode> GetSupportedModes() = 0;

        /**
         * Get a current mode
         *
         * This routine will return a current mode of the driver
         * Please see EACTDriverMode list.
         *
         * @return - a current driver mode.
         */
        virtual const EACTDriverMode GetCurrentMode() = 0;

        /**
         * @brief GetDeviceList
         * @return
         */
        virtual const std::vector<std::string> GetDeviceList() = 0;

        /**
         * Get a supported command list
         *
         * This routine will return a list of currently supported
         * commands by the driver. This information can be used
         * for choosing a right command when ProcessPacket is called.
         *
         * @return - a list of available commands
         */
        virtual const std::vector<EPacketCommand> GetCommandList() = 0;

        // Async Driver Interface
        virtual CATLError AddListerner(EACTDriverListenerTypes type, IATLObserver* proc) = 0;
        virtual CATLError RemoveListerner(EACTDriverListenerTypes type, IATLObserver* proc) = 0;
    };

    class CBaseDriver : public virtual IDriver {
    public:

         /**
          * Add a listener for callback
          *
          * Register a callback listener to get an even notifications
          *
          * @param type - type of notification to register
          * @param proc - observer object
          *
          * @return EATLErrorOk - observer has been registered
          *         EATLErrorFatal - failed to register an observer
          */
        virtual CATLError AddListerner(EACTDriverListenerTypes type, IATLObserver* proc);


        /**
         * Remove a listener from callback list
         *
         * Remove a callback listener from a list
         * to stop notifications
         *
         * @param type - type of notification to remove
         * @param proc - observer object
         *
         * @return EATLErrorOk - observer has been removed
         *         EATLErrorFatal - failed to remove an observer
         */
        virtual CATLError RemoveListerner(EACTDriverListenerTypes type, IATLObserver* proc);

    protected:
        CATLError NotifyListerners(TSmartPtr< CTransferPacket > packet);
        // callback interface for async mode
    private:
        CEventListener<> OnReplyReceived;
    };
}

#endif // __BASE_DRIVER_INTERFACE__
