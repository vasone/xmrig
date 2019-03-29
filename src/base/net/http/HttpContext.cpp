/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2014-2019 heapwolf    <https://github.com/heapwolf>
 * Copyright 2018-2019 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2019 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <algorithm>
#include <uv.h>


#include "3rdparty/http-parser/http_parser.h"
#include "base/net/http/HttpContext.h"


xmrig::HttpContext::HttpContext(int parser_type, IHttpListener *listener) :
    listener(listener),
    connect(nullptr),
    m_wasHeaderValue(false)
{
    parser = new http_parser;
    tcp    = new uv_tcp_t;

    uv_tcp_init(uv_default_loop(), tcp);
    http_parser_init(parser, static_cast<http_parser_type>(parser_type));

    parser->data = tcp->data = this;
}


xmrig::HttpContext::~HttpContext()
{
    delete connect;
    delete tcp;
    delete parser;
}


void xmrig::HttpContext::attach(http_parser_settings *settings)
{
    settings->on_message_begin  = nullptr;
    settings->on_status         = nullptr;
    settings->on_chunk_header   = nullptr;
    settings->on_chunk_complete = nullptr;

    settings->on_url = [](http_parser *parser, const char *at, size_t length) -> int
    {
        static_cast<HttpContext*>(parser->data)->url = std::string(at, length);
        return 0;
    };

    settings->on_header_field = onHeaderField;
    settings->on_header_value = onHeaderValue;

    settings->on_headers_complete = [](http_parser* parser) -> int {
        HttpContext *ctx = static_cast<HttpContext*>(parser->data);
        ctx->method = std::string(http_method_str(static_cast<http_method>(parser->method)));

        if (!ctx->m_lastHeaderField.empty()) {
            ctx->setHeader();
        }

        return 0;
    };

    settings->on_body = [](http_parser *parser, const char *at, size_t len) -> int
    {
        static_cast<HttpContext*>(parser->data)->body << std::string(at, len);

        return 0;
    };
}


void xmrig::HttpContext::close(uv_handle_t* handle)
{
    delete reinterpret_cast<HttpContext*>(handle->data);
}


int xmrig::HttpContext::onHeaderField(http_parser *parser, const char *at, size_t length)
{
    HttpContext *ctx = static_cast<HttpContext*>(parser->data);

    if (ctx->m_wasHeaderValue) {
        if (!ctx->m_lastHeaderField.empty()) {
            ctx->setHeader();
        }

        ctx->m_lastHeaderField = std::string(at, length);
        ctx->m_wasHeaderValue  = false;
    } else {
        ctx->m_lastHeaderField += std::string(at, length);
    }

    return 0;
}


int xmrig::HttpContext::onHeaderValue(http_parser *parser, const char *at, size_t length)
{
    HttpContext *ctx = static_cast<HttpContext*>(parser->data);

    if (!ctx->m_wasHeaderValue) {
        ctx->m_lastHeaderValue = std::string(at, length);
        ctx->m_wasHeaderValue  = true;
    } else {
        ctx->m_lastHeaderValue += std::string(at, length);
    }

    return 0;
}


void xmrig::HttpContext::setHeader()
{
    std::transform(m_lastHeaderField.begin(), m_lastHeaderField.end(), m_lastHeaderField.begin(), ::tolower);
    headers.insert({ m_lastHeaderField, m_lastHeaderValue });

    m_lastHeaderField.clear();
    m_lastHeaderValue.clear();
}

