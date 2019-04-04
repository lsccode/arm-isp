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

#ifndef __DRV_FTDI_H__
#define __DRV_FTDI_H__

#include <stdexcept>
#include <vector>
#include <ATL/ATLLogger.h>

#ifdef _WIN32
#include <windows.h>
#else

#endif

typedef std::vector<std::string> DevList;

#define FTDI_DIS_DIV_5      0x8A
#define FTDI_DIS_ADAPT_CLK  0x97
#define FTDI_EN_3_PHASE     0x8C
#define FTDI_DIS_LOOPBACK   0x85
#define FTDI_TCK_DIVIDER    0x86
#define FTDI_SET_BITS_LOW   0x80
#define FTDI_SET_BITS_HIGH  0x82
#define FTDI_SEND_ANSWER    0x87

#define FTDI_SEND_BYTE      0x11
#define FTDI_SEND_BIT       0x13
#define FTDI_REC_BYTE       0x20
#define FTDI_REC_BIT        0x22

// General FTDI interface
namespace act {

struct arm_device_list
{
    struct arm_device_list *mNextDevNode;
    struct arm_usb_device  *mDevNode;
};

enum arm_chip_type
{
    TYPE_ZERO = 0,
    TYPE_ONE,
    TYPE_TWO,
    TYPE_THREE,
    TYPE_FOUR,
    TYPE_FIVE,
    TYPE_SIX,
    TYPE_SEVEN
};

enum arm_detach_mode
{
    MODE_ZERO = 0,
    MODE_ONE
};

typedef char arm_char;
typedef unsigned char arm_uchar;
typedef int arm_int;
typedef unsigned int arm_uint;

struct arm_context
{
    struct arm_usb_context    *mDeviceContext;
    struct arm_usb_dev_handle *mDevice;
    arm_int                   mReadTimeout;
    arm_int                   mWriteRimeout;

    enum arm_chip_type        mChipType;
    arm_int                   mBaudRate;
    arm_uchar                 mIsBitBangEnabled;
    arm_uchar                 *mReadBuffer;
    arm_uint                  mReadBufferOffset;
    arm_uint                  mReadBufferPendingCount;
    arm_uint                  mReadBufferPacketSize;
    arm_uint                  mWriteBufferPacketSize;
    arm_uint                  mMaxPacketSize;

    arm_int                   mInterface;
    arm_int                   mIndex;
    arm_int                   mInEndPoint;
    arm_int                   mOutEndPoint;
    arm_uchar                 mBitbangMode;
    struct arm_eeprom         *mEEPROMStruct;
    arm_char                  *mErrorString;
    enum arm_detach_mode      mDetachMode;
};



class FTDI_BASE {
public:
    class Error : std::runtime_error {
    public:
        Error(const char* what) : runtime_error(what) { atl::SysLog()->LogError("FTDI", "exception %s", what); }
    };

    const std::string& get_chip_name() {return chip_name;}
protected:
    std::string chip_name;
};

#ifdef _WIN32

// Definitions for supporting FT Devices
typedef void * ARM_HANDLE;
typedef ULONG ARM_STATUS;

enum {
    ARM_OK
};

#define FTDI_BITMODE_RESET      0x0
#define FTDI_BITMODE_MPSSE      0x02
#define FTDI_BITMODE_BITBANG    0x04


// FTDI driver for D2XX
typedef ARM_STATUS (_STDCALL *f_FT_Open)(int deviceNumber,ARM_HANDLE *pHandle);
typedef ARM_STATUS (_STDCALL *f_FT_SetBaudRate)(ARM_HANDLE ftHandle,ULONG BaudRate);
typedef ARM_STATUS (_STDCALL *f_FT_SetBitMode)(ARM_HANDLE ftHandle,UCHAR ucMask,UCHAR ucEnable);
typedef ARM_STATUS (_STDCALL *f_FT_Read)(ARM_HANDLE ftHandle,LPVOID lpBuffer,DWORD dwBytesToRead,LPDWORD lpBytesReturned);
typedef ARM_STATUS (_STDCALL *f_FT_Write)(ARM_HANDLE ftHandle,LPCVOID lpBuffer,DWORD dwBytesToWrite,LPDWORD lpBytesWritten);
typedef ARM_STATUS (_STDCALL *f_FT_ResetDevice)(ARM_HANDLE ftHandle);
typedef ARM_STATUS (_STDCALL *f_FT_Close)(ARM_HANDLE ftHandle);
typedef ARM_STATUS (_STDCALL *f_FT_CreateDeviceInfoList)(LPDWORD lpdwNumDevs);
typedef ARM_STATUS (_STDCALL *f_FT_GetDeviceInfoDetail)(DWORD dwIndex,LPDWORD lpdwFlags,LPDWORD lpdwType,LPDWORD lpdwID,LPDWORD lpdwLocId,LPVOID lpSerialNumber,LPVOID lpDescription,ARM_HANDLE *pftHandle);
typedef ARM_STATUS (_STDCALL *f_FT_SetLatencyTimer)(ARM_HANDLE ftHandle,UCHAR ucLatency);
typedef ARM_STATUS (_STDCALL *f_FT_SetUSBParameters)(ARM_HANDLE ftHandle, ULONG ulInTransferSize, ULONG ulOutTransferSize);
typedef ARM_STATUS (_STDCALL *f_FT_SetTimeouts)(ARM_HANDLE ftHandle, ULONG ReadTimeout, ULONG WriteTimeout);

class FTDI : public FTDI_BASE {
public:
    static const bool read_before_write = false;
    FTDI();
    ~FTDI();
    bool open(const std::string& name);
    void close();
    void reset();
    void set_bitmode(UInt8 mask, UInt8 mode);
    void set_baudrate(UInt32 baudrate);
    void write(const std::vector<UInt8>& data);
    void read(std::vector<UInt8>& data);
    const DevList get_devices();
private:
    void* hLib;
    ARM_HANDLE hFTDI;
    f_FT_Open FT_Open;
    f_FT_SetBaudRate FT_SetBaudRate;
    f_FT_SetBitMode FT_SetBitMode;
    f_FT_Read FT_Read;
    f_FT_Write FT_Write;
    f_FT_ResetDevice FT_ResetDevice;
    f_FT_Close FT_Close;
    f_FT_CreateDeviceInfoList FT_CreateDeviceInfoList;
    f_FT_GetDeviceInfoDetail FT_GetDeviceInfoDetail;
    f_FT_SetLatencyTimer FT_SetLatencyTimer;
    f_FT_SetUSBParameters FT_SetUSBParameters;
    f_FT_SetTimeouts FT_SetTimeouts;
};

#else

#define FTDI_BITMODE_RESET      0x00
#define FTDI_BITMODE_MPSSE      0x02
#define FTDI_BITMODE_BITBANG    0x04

typedef struct arm_context* (*f_context_void)(void);
typedef void (*f_void_context)(struct arm_context *);
typedef int (*f_int_context)(struct arm_context *);
typedef int (*f_int_context_int)(struct arm_context *, int);
typedef int (*f_int_context_int_int)(struct arm_context *, int, int);
typedef int (*f_int_context_char_char)(struct arm_context *, unsigned char, unsigned char);
typedef int (*f_int_context_pchar_int)(struct arm_context *, unsigned char*, int);
typedef int (*f_int_context_cpchar_int)(struct arm_context *, const unsigned char*, int);
typedef int (*f_get_strings)(struct arm_context *, struct arm_usb_device *,char *, int,char *, int,char *, int);
typedef int (*f_find_all) (struct arm_context *, struct arm_device_list **, int, int);
typedef void (*f_pplist) (struct arm_device_list **);
typedef int (*f_int_context_pdev)(struct arm_context *, struct arm_usb_device *);
typedef int (*f_int_context_char)(struct arm_context *, unsigned char);
typedef int (*f_int_context_uint)(struct arm_context *, unsigned int);

class FTDI : public FTDI_BASE {
public:
    static const bool read_before_write = true;
    FTDI();
    ~FTDI();
    bool open(const std::string& name);
    void close();
    void reset();
    void set_bitmode(UInt8 mask, UInt8 mode);
    void set_baudrate(UInt32 baudrate);
    void write(const std::vector<UInt8>& data);
    void read(std::vector<UInt8>& data);
    const DevList get_devices();
private:
    void detach();
    struct arm_context* hFTDI;
    void* hLib;
    f_context_void ftdi_new;
    f_void_context ftdi_free;
    f_int_context_int_int ftdi_usb_open;
    f_int_context ftdi_usb_close;
    f_int_context ftdi_usb_reset;
    f_int_context_int ftdi_set_baudrate;
    f_int_context_char_char ftdi_set_bitmode;
    f_int_context_pchar_int ftdi_read_data;
    f_int_context_cpchar_int ftdi_write_data;
    f_int_context ftdi_usb_purge_buffers;
    f_find_all ftdi_usb_find_all ;
    f_pplist ftdi_list_free;
    f_int_context_pdev ftdi_usb_open_dev;
    f_int_context_char ftdi_set_latency_timer;
    f_get_strings ftdi_usb_get_strings;
    f_int_context_uint ftdi_write_data_set_chunksize;
    f_int_context_uint ftdi_read_data_set_chunksize;
};
#endif

}
#endif /* __DRV_FTDI_H__ */
