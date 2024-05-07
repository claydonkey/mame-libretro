// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#ifndef VECTOR_TCP_DVG_H
#define VECTOR_TCP_DVG_H
#pragma once

#include "osdcore.h"
#include "screen.h"
#include "divector.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include "msgpack/msgpack.hpp"
#include "vector_udp_dvg.h"


#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif



#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#define MAX_JSON_SIZE           512
#define BUFF_SIZE               1470U
#define SIZEOF_LEN 4
#define SIZEOF_HEADER 9
#define SIZEOF_PKT_CTR 2
#define MSG_IDX_START           5
#define DVG_RELEASE             0
#define DVG_BUILD               1
#define CMD_BUF_SIZE            0x20000
#define DVG_RES_MIN              0
#define DVG_RES_MAX              4095
#define SAVE_TO_FILE             0
#define SORT_VECTORS             0
#define MAX_VECTORS              0x10000
#define MAX_JSON_SIZE            512
#define GAME_NONE                0
#define GAME_ARMORA              1
#define GAME_WARRIOR             2
#define CMD_LEN 6

 

using namespace std;

 

class vector_device_tcp_dvg : public vector_device_udp_dvg
{

public:


	vector_device_tcp_dvg(const machine_config& mconfig, device_type type,const char* tag, device_t* owner, uint32_t clock = 0);


private:
 
 


protected:
	virtual void device_start() override;



};

// device type definition
DECLARE_DEVICE_TYPE(VECTOR_TCP_DVG, vector_device_tcp_dvg)
#endif // VECTOR_USB_DVG_H
