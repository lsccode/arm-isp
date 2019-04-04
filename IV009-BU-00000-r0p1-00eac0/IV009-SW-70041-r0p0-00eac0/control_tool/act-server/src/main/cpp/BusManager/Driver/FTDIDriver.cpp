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

#ifndef _ANDROID // this is not supported on Android platform
#include <string.h>

#include <ATL/ATLLogger.h>
#include <ATL/ATLLibrary.h>

using namespace std;
using namespace atl;

#define LOG_LIBFTDI "FTDI"
#define LOG_LIBD2XX "FTDI"
#include "FTDIDriver.h"

namespace act {

#ifdef _WIN32
// FTDI driver for D2XX

FTDI::FTDI()
{
    hFTDI = 0; // device is closed
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to load dynamic  library...");
    if (!(hLib = CATLLibrary::LoadSharedLibrary("ftd2xx.dll")))
    {
        SysLog()->LogInfo(LOG_LIBD2XX, "dynamic library can't' be loaded");
        throw Error("can't load dynamic library ftd2xx.dll");
    }
    SysLog()->LogDebug(LOG_LIBD2XX, "dynamic library is loaded");
    SysLog()->LogDebug(LOG_LIBD2XX, "loading functions from dynamic library...");
    if (!(FT_Open = (f_FT_Open)CATLLibrary::GetFunction(hLib,"FT_Open")) ||
        !(FT_SetBaudRate = (f_FT_SetBaudRate)CATLLibrary::GetFunction(hLib,"FT_SetBaudRate")) ||
        !(FT_SetBitMode = (f_FT_SetBitMode)CATLLibrary::GetFunction(hLib,"FT_SetBitMode")) ||
        !(FT_Read = (f_FT_Read)CATLLibrary::GetFunction(hLib,"FT_Read")) ||
        !(FT_Write = (f_FT_Write)CATLLibrary::GetFunction(hLib,"FT_Write")) ||
        !(FT_ResetDevice = (f_FT_ResetDevice)CATLLibrary::GetFunction(hLib,"FT_ResetDevice")) ||
        !(FT_CreateDeviceInfoList = (f_FT_CreateDeviceInfoList)CATLLibrary::GetFunction(hLib,"FT_CreateDeviceInfoList")) ||
        !(FT_GetDeviceInfoDetail = (f_FT_GetDeviceInfoDetail)CATLLibrary::GetFunction(hLib,"FT_GetDeviceInfoDetail")) ||
        !(FT_SetLatencyTimer = (f_FT_SetLatencyTimer)CATLLibrary::GetFunction(hLib,"FT_SetLatencyTimer")) ||
        !(FT_SetUSBParameters = (f_FT_SetUSBParameters)CATLLibrary::GetFunction(hLib,"FT_SetUSBParameters")) ||
        !(FT_SetTimeouts = (f_FT_SetTimeouts)CATLLibrary::GetFunction(hLib,"FT_SetTimeouts")) ||
        !(FT_Close = (f_FT_Close)CATLLibrary::GetFunction(hLib,"FT_Close")))
    {
        CATLLibrary::FreeSharedLibrary(hLib);
        throw Error("can't load functions from dynamic library");
    }
    SysLog()->LogDebug(LOG_LIBD2XX, "functions are loaded from dynamic  library");
}

FTDI::~FTDI()
{
    if (hFTDI) close();
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to unload dynamic library");
    if (CATLLibrary::FreeSharedLibrary(hLib)) SysLog()->LogDebug(LOG_LIBD2XX, "dynamic library has been unloaded");
    else SysLog()->LogError(LOG_LIBD2XX,"wrong dynamic library unloading");
}


bool FTDI::open(const std::string& name)
{
    ARM_STATUS ftStatus;
    ARM_HANDLE ftHandleTemp;
    DWORD numDevs;
    DWORD Flags;
    DWORD ID;
    DWORD Type;
    DWORD LocId;
    char SerialNumber[16];
    char Description[64];
    if (hFTDI) {
        throw Error("USB device is already opened");
    }
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to open USB device %s",name.c_str());
    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus == ARM_OK) {
        for (DWORD i = 0; i < numDevs; i++) {
            ftStatus = FT_GetDeviceInfoDetail(i, &Flags, &Type, &ID, &LocId, SerialNumber, Description, &ftHandleTemp);
            if (ftStatus == ARM_OK) {
                if (name == string("FTDI,")+Description+","+SerialNumber) {
                    ARM_STATUS res = FT_Open(i,&hFTDI);
                    if (res!=ARM_OK) {
                        hFTDI = 0;
                        SysLog()->LogInfo(LOG_LIBD2XX, "can't open USB device: error: %d", res);
                        return false;
                    }
                    chip_name = Description;
                    if (ARM_OK != FT_SetLatencyTimer(hFTDI,1) ||
                        ARM_OK != FT_SetUSBParameters(hFTDI,65536,65536) ||
                        ARM_OK != FT_SetTimeouts(hFTDI,5000,1000)) {
                        SysLog()->LogWarning(LOG_LIBD2XX,"can't set FTDI parameters");
                    }
                    SysLog()->LogInfo(LOG_LIBD2XX, "USB device opened successfully");
                    return true;
                }
            }
        }
    }

    SysLog()->LogWarning(LOG_LIBD2XX, "can't find device %s", name.c_str());
    return false;
}

void FTDI::close()
{
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to close USB device...");
    if (hFTDI) {
        ARM_STATUS res = FT_Close(hFTDI);
        if (res == ARM_OK)
            SysLog()->LogInfo(LOG_LIBD2XX, "USB device has been closed successfully");
        else
            SysLog()->LogError(LOG_LIBD2XX, "can't close USB device: error: %d", res);
        hFTDI = 0;
    } else {
        SysLog()->LogError(LOG_LIBD2XX,"USB device is already closed");
    }
}

void FTDI::reset()
{
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to reset USB device");
    if (hFTDI) {
        ARM_STATUS res = FT_ResetDevice(hFTDI);
        if (ARM_OK != res) {
            throw Error("can't reset USB device");
        }
        SysLog()->LogDebug(LOG_LIBD2XX, "USB device has been reset");
    } else {
        throw Error("attempt to reset closed device");
    }
}

void FTDI::set_bitmode(UInt8 mask, UInt8 mode)
{
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to set bitmode...");
    if (hFTDI) {
        ARM_STATUS res = FT_SetBitMode(hFTDI, mask, mode);
        if (ARM_OK != res) {
            throw Error("can't set bitmode for USB device");
        }
        SysLog()->LogDebug(LOG_LIBD2XX, "USB device bitmode is set: mask: 0x%02X mode: 0x%02X", mask, mode);
    } else {
        throw Error("attempt to set bitmode for closed device");
    }
}

void FTDI::set_baudrate(UInt32 baudrate)
{
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to set baud rate...");
    if (hFTDI) {
        ARM_STATUS res = FT_SetBaudRate(hFTDI, baudrate);
        if (ARM_OK != res) {
            throw Error("can't set baud rate for USB device");
        }
        SysLog()->LogInfo(LOG_LIBD2XX, "USB device baud rate has been set to %d", baudrate);
    } else {
        throw Error("attempt to set baud rate for closed device");
    }
}

void FTDI::write(const std::vector<UInt8>& data)
{
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to write data to USB device...");
    if (hFTDI) {
        DWORD total = 0;
        ARM_STATUS res;
        do {
            DWORD written = 0;
            res = FT_Write(hFTDI,const_cast<UInt8*>(data.data())+total,(DWORD)data.size()-total,&written);
            SysLog()->LogDebug(LOG_LIBFTDI, "USB write portion started from " FSIZE_T " out of " FSIZE_T " requested: " FSIZE_T " written " FSIZE_T " status %d", total, (DWORD)data.size(), (DWORD)data.size()-total, written, res);
            total += written;
        } while (res == ARM_OK && data.size() != total);
        if (res != ARM_OK) {
            throw Error("Can't write to device");
        }
        SysLog()->LogDebug(LOG_LIBD2XX, "USB write: requested: " FSIZE_T " bytes -  written " FSIZE_T " bytes", data.size(), total);
        if (data.size() != total) {
            throw Error( "USB write less data then requested");
        }
    } else {
        throw Error("trying to write to closed device");
    }
}

void FTDI::read(std::vector<UInt8>& data)
{
    SysLog()->LogDebug(LOG_LIBD2XX,"trying to read data from USB device...");
    if (hFTDI) {
        DWORD total = 0;
        ARM_STATUS res;
        do {
            DWORD read;
            res = FT_Read(hFTDI,data.data()+total,(DWORD)data.size()-total,&read);
            SysLog()->LogDebug(LOG_LIBFTDI, "USB read portion started from " FSIZE_T " out of " FSIZE_T ": requested: " FSIZE_T " bytes - read: " FSIZE_T " bytes status %d", total, (DWORD)data.size(), (DWORD)data.size()-total, read, res);
            total += read;
        } while (res == ARM_OK && data.size() != total);
        if (res != ARM_OK) {
            data.clear();
            throw Error("can't read from USB device");
        }
        SysLog()->LogDebug(LOG_LIBD2XX, "USB read: requested: " FSIZE_T " bytes - read " FSIZE_T " bytes", data.size(), total);
        if (total != data.size()) {
            SysLog()->LogWarning(LOG_LIBD2XX,"USB read less data then requested");
            data.resize(total);
        }
    }
    else{
        throw Error("Trying to write to non-opened device");
    }
}

const DevList FTDI::get_devices()
{
    DevList devices;
    if (hFTDI) {
        throw Error("USB device is already opened");
    }
    ARM_STATUS ftStatus;
    ARM_HANDLE ftHandleTemp;
    DWORD numDevs;
    DWORD Flags;
    DWORD ID;
    DWORD Type;
    DWORD LocId;
    char SerialNumber[16];
    char Description[64];
    // create the device information list
    SysLog()->LogDebug(LOG_LIBD2XX, "trying to enumerate USB devices...");
    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus == ARM_OK) {
        SysLog()->LogInfo(LOG_LIBD2XX, "number of USB devices is %u", numDevs);
        for (DWORD i = 0; i < numDevs; i++) {
            ftStatus = FT_GetDeviceInfoDetail(i, &Flags, &Type, &ID, &LocId, SerialNumber, Description, &ftHandleTemp);
            if (ftStatus == ARM_OK && strlen(Description)) {
                SysLog()->LogInfo(LOG_LIBD2XX, "found device: description: %s, serial %s", Description, SerialNumber);
                devices.push_back(string("FTDI,")+Description+","+SerialNumber);
            }
        }
    }
    return devices;
}

#else

FTDI::FTDI()
{
    hFTDI = 0;
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to load shared library...");
    if (!(hLib = CATLLibrary::LoadSharedLibrary("libftdi.so")) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("libftdi1.so")) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("libftdi.so.1")) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("libftdi1.so.2")) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("./libftdi.so")) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("./libftdi1.so")) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("./libftdi.so.1.20.0",RTLD_GLOBAL)) &&
        !(hLib = CATLLibrary::LoadSharedLibrary("./libftdi1.so.2.1.0",RTLD_GLOBAL)))
    {
        SysLog()->LogInfo(LOG_LIBFTDI, "library is NOT loaded");
        throw Error("can't load shared library libftdi1.so");
    }
    SysLog()->LogDebug(LOG_LIBFTDI, "shared library has been loaded");
    SysLog()->LogDebug(LOG_LIBFTDI, "loading functions from shared library");
    if (!(ftdi_new = (f_context_void)CATLLibrary::GetFunction(hLib,"ftdi_new")) ||
        !(ftdi_free = (f_void_context)CATLLibrary::GetFunction(hLib,"ftdi_free")) ||
        !(ftdi_usb_open = (f_int_context_int_int)CATLLibrary::GetFunction(hLib,"ftdi_usb_open")) ||
        !(ftdi_usb_close = (f_int_context)CATLLibrary::GetFunction(hLib,"ftdi_usb_close")) ||
        !(ftdi_usb_reset = (f_int_context)CATLLibrary::GetFunction(hLib,"ftdi_usb_reset")) ||
        !(ftdi_set_baudrate = (f_int_context_int)CATLLibrary::GetFunction(hLib,"ftdi_set_baudrate")) ||
        !(ftdi_set_bitmode = (f_int_context_char_char)CATLLibrary::GetFunction(hLib,"ftdi_set_bitmode")) ||
        !(ftdi_read_data = (f_int_context_pchar_int)CATLLibrary::GetFunction(hLib,"ftdi_read_data")) ||
        !(ftdi_write_data = (f_int_context_cpchar_int)CATLLibrary::GetFunction(hLib,"ftdi_write_data")) ||
        !(ftdi_usb_purge_buffers = (f_int_context)CATLLibrary::GetFunction(hLib,"ftdi_usb_purge_buffers")) ||
        !(ftdi_usb_find_all = (f_find_all)CATLLibrary::GetFunction(hLib,"ftdi_usb_find_all")) ||
        !(ftdi_list_free = (f_pplist)CATLLibrary::GetFunction(hLib,"ftdi_list_free")) ||
        !(ftdi_usb_open_dev = (f_int_context_pdev)CATLLibrary::GetFunction(hLib,"ftdi_usb_open_dev")) ||
        !(ftdi_set_latency_timer = (f_int_context_char)CATLLibrary::GetFunction(hLib,"ftdi_set_latency_timer")) ||
        !(ftdi_write_data_set_chunksize = (f_int_context_uint)CATLLibrary::GetFunction(hLib,"ftdi_write_data_set_chunksize")) ||
        !(ftdi_read_data_set_chunksize = (f_int_context_uint)CATLLibrary::GetFunction(hLib,"ftdi_read_data_set_chunksize")) ||
        !(ftdi_usb_get_strings = (f_get_strings)CATLLibrary::GetFunction(hLib,"ftdi_usb_get_strings")))
    {
        CATLLibrary::FreeSharedLibrary(hLib);
        throw Error("can't load functions from shared library");
    }
    SysLog()->LogDebug(LOG_LIBFTDI, "functions are loaded from shared library");
}

FTDI::~FTDI()
{
    if (hFTDI) close();
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to unload library");
    if (CATLLibrary::FreeSharedLibrary(hLib)) SysLog()->LogDebug(LOG_LIBFTDI, "shared library has been unloaded");
    else SysLog()->LogWarning(LOG_LIBFTDI, "error when unloading shared library");
}

bool FTDI::open(const string& name)
{
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to open USB device %s", name.c_str());
    if (hFTDI) {
        throw Error("USB device is already opened");
    }
    hFTDI = ftdi_new();
    if (!hFTDI) {
        throw Error("can't create USB device context");
    }
    // how many devices are connected?
    struct arm_device_list * dl;
    int num = ftdi_usb_find_all(hFTDI,&dl,0,0);
    SysLog()->LogDebug(LOG_LIBFTDI, "number of devices is %d", num );
    if (num >= 1) {
        for (struct arm_device_list * dev = dl; dev; dev = dev->mNextDevNode) {
            char manufacturer[100] = {0};
            char description[100] = {0};
            char serial[100] = {0};
            ftdi_usb_get_strings(hFTDI, dev->mDevNode, manufacturer, sizeof(manufacturer), description, sizeof(description), serial, sizeof(serial));
            string dev_name = string(manufacturer)+","+description+","+serial;
            SysLog()->LogDebug(LOG_LIBFTDI, "found device %s", dev_name.c_str());
            if (name == dev_name) {
                if (0!=ftdi_usb_open_dev(hFTDI,dev->mDevNode)) {
                    detach();
                    throw Error("can't open USB device");
                }
                ftdi_list_free(&dl);
                chip_name = description;

                if (ftdi_set_latency_timer(hFTDI,1) < 0 ||
                    ftdi_write_data_set_chunksize(hFTDI,65536) < 0 ||
                    ftdi_read_data_set_chunksize(hFTDI,65536) < 0) {
                    SysLog()->LogWarning(LOG_LIBFTDI, "can't set up USB device %s", name.c_str());
                } else {
                    SysLog()->LogDebug(LOG_LIBFTDI, "USB device %s is set up", name.c_str());
                }
                SysLog()->LogInfo(LOG_LIBFTDI, "USB device %s is opened", name.c_str());
                return true;
            }
        }
        ftdi_list_free(&dl);
        detach();
        SysLog()->LogError(LOG_LIBFTDI, "can't find USB device %s", name.c_str());
        return false;
    } else if (num == 0) {
        SysLog()->LogError(LOG_LIBFTDI, "USB device %s is not connected", name.c_str());
        ftdi_list_free(&dl);
        detach();
        return false;
    }
    ftdi_list_free(&dl);
    detach();
    throw Error("can't search devices");
}

void FTDI::detach()
{
    ftdi_free(hFTDI);
    hFTDI = 0;
}

void FTDI::close()
{
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to close USB device...");
    if (hFTDI) {
        ftdi_usb_close(hFTDI);
        detach();
        SysLog()->LogInfo(LOG_LIBFTDI, "USB device is closed successfully");
    } else {
        SysLog()->LogWarning(LOG_LIBFTDI, "USB device is already closed");
    }
}

void FTDI::reset()
{
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to reset USB device...");
    if (hFTDI) {
        if (ftdi_usb_reset(hFTDI) || ftdi_usb_purge_buffers(hFTDI)) {
            throw Error("Can't reset USB device");
        }
        SysLog()->LogDebug(LOG_LIBFTDI, "USB device is reset");
    } else {
        throw Error("attempt to reset closed device");
    }
}

void FTDI::set_bitmode(UInt8 mask, UInt8 mode)
{
    SysLog()->LogDebug(LOG_LIBFTDI, "try to set bitmode...");
    if (hFTDI) {
        if (ftdi_set_bitmode(hFTDI,mask,mode)) {
            throw Error("Can't set bitmode for USB device");
        }
        SysLog()->LogDebug(LOG_LIBFTDI, "USB device bitmode is set, mask: 0x%02X mode: 0x%02X",mask,mode);
    } else {
        throw Error("attempt to set bitmode for closed device");
    }
}

void FTDI::set_baudrate(UInt32 baudrate)
{
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to set baud rate");
    if (hFTDI) {
        if (ftdi_set_baudrate(hFTDI,baudrate)) {
            throw Error("can't set baudrate for USB device");
        }
        SysLog()->LogDebug(LOG_LIBFTDI, "USB device baudrate is set to %d",baudrate);
    } else {
        throw Error("attempt to set baudrate cor closed device");
    }
}

void FTDI::write(const std::vector<UInt8>& data)
{
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to write data to USB device");
    if (hFTDI) {
        int total = 0;
        int res;
        do {
            res = ftdi_write_data(hFTDI,data.data()+total,(int)data.size()-total);
            SysLog()->LogDebug(LOG_LIBFTDI, "USB write portion started from %d out of %d requested: %d written %d", total, (int)data.size(), (int)data.size()-total, res);
            if (res > 0) total += res;
        } while (res > 0 && total != (int)data.size());
        SysLog()->LogDebug(LOG_LIBFTDI, "USB write requested: " FSIZE_T " bytes - written: %d bytes", data.size(), total);
        if (res < 0) {
            throw Error("Can't write to device");
        }
        if ((UInt32)total != data.size()) {
            throw Error("USB write less data then requested");
        }
    } else {
        throw Error("attempt to write to closed device");
    }
}

void FTDI::read(std::vector<UInt8>& data)
{
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to read data from USB device");
    if (hFTDI) {
        basesize total = 0;
        int cnt = 10;
        int res;
        do {
            res = ftdi_read_data(hFTDI,data.data()+total,(int)data.size()-total);
            SysLog()->LogDebug(LOG_LIBFTDI, "USB read portion started from %d out of %d requested: %d read %d", total, (int)data.size(), (int)data.size()-total, res);
            if (res > 0) total += res;
        } while (res >= 0 && total != data.size() && cnt--);
        SysLog()->LogDebug(LOG_LIBFTDI, "USB read, requested: " FSIZE_T " read %d cnt %d", data.size(), total, cnt);
        if (res < 0) {
            data.clear();
            throw Error("can't read from device");
        }
        if (total != data.size()) {
            SysLog()->LogWarning(LOG_LIBFTDI, "USB read size is incorrect");
            data.resize(total);
        }
    } else{
        throw Error("attempt to read from closed device");
    }
}

const DevList FTDI::get_devices()
{
    DevList devices;
    SysLog()->LogDebug(LOG_LIBFTDI, "trying to list attached devices");
    if (hFTDI) {
        throw Error("USB device is already opened");
    }
    struct arm_context* hFTDI = ftdi_new();
    if (!hFTDI) {
        throw Error("can't create context");
    }
    struct arm_device_list * dl;
    int num = ftdi_usb_find_all(hFTDI,&dl,0,0);
    if (num >= 1) {
        SysLog()->LogDebug(LOG_LIBFTDI, "found following USB devices:");
        for (struct arm_device_list * dev = dl; dev; dev = dev->mNextDevNode) {
            char manufacturer[100] = {0};
            char description[100] = {0};
            char serial[100] = {0};
            ftdi_usb_get_strings(hFTDI, dev->mDevNode, manufacturer, sizeof(manufacturer), description, sizeof(description), serial, sizeof(serial));
            SysLog()->LogDebug(LOG_LIBFTDI, "manufacturer: %s description: %s serial: %s", manufacturer, description, serial);
            if (strlen(description)) {
                devices.push_back(string(manufacturer)+","+description+","+serial);
            }
        }
    }
    ftdi_list_free(&dl);
    ftdi_free(hFTDI);
    return devices;
}

#endif //_WIN32

}
#endif // !_ANDROID
