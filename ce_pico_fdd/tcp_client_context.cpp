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

class ClientContext;
class WiFiClient;

#include <stdio.h>
#include <string.h>
#include "pico/time.h"

#include <assert.h>
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcp.h "
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "tcp_client_context.h"
#include "utils.h"

ClientContext::ClientContext(void)
{
    setPcb(nullptr);
}

void ClientContext::setPcb(tcp_pcb* pcb)
{
    _pcb = pcb;
    _rx_buf = 0;
    _rx_buf_offset = 0;
}

err_t ClientContext::abort() {
    if (_pcb) {
        // printf(":abort\r\n");
        tcp_arg(_pcb, nullptr);
        tcp_sent(_pcb, nullptr);
        tcp_recv(_pcb, nullptr);
        tcp_err(_pcb, nullptr);
        tcp_poll(_pcb, nullptr, 0);
        tcp_abort(_pcb);
        _pcb = nullptr;
    }
    return ERR_ABRT;
}

err_t ClientContext::close() {
    err_t err = ERR_OK;
    if (_pcb) {
        // printf(":close\r\n");
        tcp_arg(_pcb, nullptr);
        tcp_sent(_pcb, nullptr);
        tcp_recv(_pcb, nullptr);
        tcp_err(_pcb, nullptr);
        tcp_poll(_pcb, nullptr, 0);
        err = tcp_close(_pcb);
        if (err != ERR_OK) {
            // printf(":tc err %d\r\n", (int) err);
            tcp_abort(_pcb);
            err = ERR_ABRT;
        }
        _pcb = nullptr;
    }
    return err;
}

ClientContext::~ClientContext()
{
}

size_t ClientContext::getSize()
{
    if (!_rx_buf) {
        return 0;
    }

    return _rx_buf->tot_len - _rx_buf_offset;
}

uint8_t ClientContext::read()
{
    if (!_rx_buf) {
        return 0;
    }

    char c = reinterpret_cast<char*>(_rx_buf->payload)[_rx_buf_offset];
    _consume(1);
    return c;
}

size_t ClientContext::read(uint8_t* dst, size_t size, uint32_t timeoutMs)
{
    uint32_t startTime = millis();
    size_t bytesGot = 0;

    while(size > 0 && (millis() - startTime) < timeoutMs)
    {
        uint32_t available = getSize();
        if(available == 0)      // no data? wait and try again
        {
            sleep_ms(1);
            continue;
        }

        size_t readSize = MIN(available, size);
        size_t realReadCnt = read(dst, readSize);
        
        size -= realReadCnt;
        dst += realReadCnt;
        bytesGot += realReadCnt;
    }

    return bytesGot;
}

size_t ClientContext::read(uint8_t* dst, size_t size) 
{
    if (!_rx_buf) {
        return 0;
    }

    size_t max_size = _rx_buf->tot_len - _rx_buf_offset;
    size = (size < max_size) ? size : max_size;

    // printf(":rd %d, %d, %d\r\n", size, _rx_buf->tot_len, _rx_buf_offset);
    size_t size_read = 0;
    while (size) {
        size_t buf_size = _rx_buf->len - _rx_buf_offset;
        size_t copy_size = (size < buf_size) ? size : buf_size;
        // printf(":rdi %d, %d\r\n", buf_size, copy_size);
        memcpy(dst, reinterpret_cast<char*>(_rx_buf->payload) + _rx_buf_offset, copy_size);
        dst += copy_size;
        _consume(copy_size);
        size -= copy_size;
        size_read += copy_size;
    }
    return size_read;
}

void ClientContext::_consume(size_t size) {
    ptrdiff_t left = _rx_buf->len - _rx_buf_offset - size;
    if (left > 0) {
        _rx_buf_offset += size;
    } else if (!_rx_buf->next) {
        // printf(":c0 %d, %d\r\n", size, _rx_buf->tot_len);
        auto head = _rx_buf;
        _rx_buf = 0;
        _rx_buf_offset = 0;
        pbuf_free(head);
    } else {
        // printf(":c %d, %d, %d\r\n", size, _rx_buf->len, _rx_buf->tot_len);
        auto head = _rx_buf;
        _rx_buf = _rx_buf->next;
        _rx_buf_offset = 0;
        pbuf_ref(_rx_buf);
        pbuf_free(head);
    }
    if (_pcb) {
        tcp_recved(_pcb, size);
    }
}

err_t ClientContext::_recv(tcp_pcb* pcb, pbuf* pb, err_t err) {
    (void) pcb;
    (void) err;
    if (pb == 0) {
        // connection closed by peer
        // printf(":rcl pb=%p sz=%d\r\n", _rx_buf, _rx_buf ? _rx_buf->tot_len : -1);
        if (_rx_buf && _rx_buf->tot_len) {
            // there is still something to read
            return ERR_OK;
        } else {
            // nothing in receive buffer,
            // peer closed = nothing can be written:
            // closing in the legacy way
            abort();
            return ERR_ABRT;
        }
    }

    if (_rx_buf) {
        // printf(":rch %d, %d\r\n", _rx_buf->tot_len, pb->tot_len);
        pbuf_cat(_rx_buf, pb);
    } else {
        // printf(":rn %d\r\n", pb->tot_len);
        _rx_buf = pb;
        _rx_buf_offset = 0;
    }
    return ERR_OK;
}

void ClientContext::_error(err_t err) {
    (void) err;
    // printf(":er %d 0x%08lx\r\n", (int) err, (uint32_t) _datasource);
    tcp_arg(_pcb, nullptr);
    tcp_sent(_pcb, nullptr);
    tcp_recv(_pcb, nullptr);
    tcp_err(_pcb, nullptr);
    _pcb = nullptr;
}
