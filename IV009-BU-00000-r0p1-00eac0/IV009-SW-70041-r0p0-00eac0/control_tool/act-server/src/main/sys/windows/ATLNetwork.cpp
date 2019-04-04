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

#include <ATL/ATLNetwork.h>

#include <winsock.h>

using namespace atl;

std::string network::getIPAddress(const std::string& defaultAddress) {
    if (defaultAddress == "0.0.0.0") {
        std::string ext_ip_addr = "xxx.xxx.xxx.xxx";
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        char ac[80];
        if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
            WSACleanup();
            return ext_ip_addr;
        }
        struct hostent *phe = gethostbyname(ac);
        if (phe == 0) {
            WSACleanup();
            return ext_ip_addr;
        }
        for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
            struct in_addr addr;
            memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
            ext_ip_addr.assign(inet_ntoa(addr));
            break;
        }
        WSACleanup();
        return ext_ip_addr;
    } else {
        return defaultAddress;
    }
}
