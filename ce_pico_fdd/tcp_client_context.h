#ifndef __TCP_CLIENT_CONTEXT_H__
#define __TCP_CLIENT_CONTEXT_H__

/*
    ClientContext.h - TCP connection handling on top of lwIP

    Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
    This file is part of the esp8266 core for Arduino environment.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

class ClientContext;
class WiFiClient;

typedef void (*discard_cb_t)(void*, ClientContext*);

#include <assert.h>
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcp.h "
#include "lwip/sockets.h"
#include "lwip/inet.h"

class ClientContext {
public:
    ClientContext();
    void setPcb(tcp_pcb* pcb);
    ~ClientContext();

    err_t abort();
    err_t close();

    size_t getSize();
    uint8_t read();
    size_t read(uint8_t* dst, size_t size);
    size_t read(uint8_t* dst, size_t size, uint32_t timeoutMs);

    void _consume(size_t size);
    err_t _recv(tcp_pcb* pcb, pbuf* pb, err_t err);
    void _error(err_t err);

private:
    tcp_pcb* _pcb;

    pbuf* _rx_buf;
    size_t _rx_buf_offset;

    const char* _datasource = nullptr;
    size_t _datalen = 0;
    size_t _written = 0;
    uint32_t _timeout_ms = 5000;
    uint32_t _op_start_time = 0;
    bool _send_waiting = false;
    bool _connect_pending = false;
};

#endif
