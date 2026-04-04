/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef USE_LIBUV

    #include "LibuvLoop.h"

    #include "../core/Memory.hpp"

    #include <uv.h>

namespace OpenRCT2::Platform
{
    LibuvLoop::LibuvLoop()
    {
        _loop = uv_default_loop();
    }

    LibuvLoop::~LibuvLoop()
    {
        // No need to close default loop or delete it
    }

    LibuvLoop& LibuvLoop::Get()
    {
        static LibuvLoop instance;
        return instance;
    }

    void LibuvLoop::Run(uv_run_mode mode)
    {
        uv_run(_loop, mode);
    }

    struct AsyncReadData
    {
        uv_fs_t open_req;
        uv_fs_t read_req;
        uv_fs_t close_req;
        uv_fs_t stat_req;
        uv_buf_t iov;
        std::string path;
        std::vector<uint8_t> buffer;
        uint64_t offset = 0;
        std::function<void(std::vector<uint8_t>, int)> callback;
        bool open_req_used = false;
        bool read_req_used = false;
        bool close_req_used = false;
        bool stat_req_used = false;

        AsyncReadData(const std::string& p, std::function<void(std::vector<uint8_t>, int)> cb)
            : path(p)
            , callback(std::move(cb))
        {
            std::memset(&open_req, 0, sizeof(open_req));
            std::memset(&read_req, 0, sizeof(read_req));
            std::memset(&close_req, 0, sizeof(close_req));
            std::memset(&stat_req, 0, sizeof(stat_req));
        }
    };

    static void on_read(uv_fs_t* req);

    static void on_close(uv_fs_t* req)
    {
        auto* data = static_cast<AsyncReadData*>(req->data);
        if (data->open_req_used)
            uv_fs_req_cleanup(&data->open_req);
        if (data->stat_req_used)
            uv_fs_req_cleanup(&data->stat_req);
        if (data->read_req_used)
            uv_fs_req_cleanup(&data->read_req);
        if (data->close_req_used)
            uv_fs_req_cleanup(&data->close_req);
        delete data;
    }

    static void on_read(uv_fs_t* req)
    {
        auto* data = static_cast<AsyncReadData*>(req->data);
        if (req->result < 0)
        {
            data->callback({}, static_cast<int>(req->result));
            data->close_req_used = true;
            int r = uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            if (r != 0)
            {
                // Synchronous error - close failed, cleanup manually
                on_close(&data->close_req);
            }
        }
        else if (req->result == 0)
        {
            // End of file - resize buffer to actual bytes read
            data->buffer.resize(data->offset);
            data->callback(std::move(data->buffer), 0);
            data->close_req_used = true;
            int r = uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            if (r != 0)
            {
                // Synchronous error - close failed, cleanup manually
                on_close(&data->close_req);
            }
        }
        else
        {
            data->offset += req->result;
            if (data->offset < data->buffer.size())
            {
                data->iov = uv_buf_init(
                    reinterpret_cast<char*>(data->buffer.data() + data->offset),
                    static_cast<unsigned int>(data->buffer.size() - data->offset));
                data->read_req_used = true;
                int r = uv_fs_read(
                    LibuvLoop::Get().GetLoop(), &data->read_req, data->open_req.result, &data->iov, 1, data->offset, on_read);
                if (r != 0)
                {
                    // Synchronous error - queue operation failed
                    data->callback({}, r);
                    if (data->open_req_used)
                        uv_fs_req_cleanup(&data->open_req);
                    if (data->stat_req_used)
                        uv_fs_req_cleanup(&data->stat_req);
                    if (data->read_req_used)
                        uv_fs_req_cleanup(&data->read_req);
                    delete data;
                }
            }
            else
            {
                data->callback(std::move(data->buffer), 0);
                data->close_req_used = true;
                int r = uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
                if (r != 0)
                {
                    // Synchronous error - close failed, cleanup manually
                    on_close(&data->close_req);
                }
            }
        }
    }

    static void on_stat(uv_fs_t* stat_req)
    {
        auto* data = static_cast<AsyncReadData*>(stat_req->data);
        if (stat_req->result < 0)
        {
            data->callback({}, static_cast<int>(stat_req->result));
            data->close_req_used = true;
            int r = uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            if (r != 0)
            {
                // Synchronous error - close failed, cleanup manually
                on_close(&data->close_req);
            }
            return;
        }

        uint64_t size = stat_req->statbuf.st_size;
        data->buffer.resize(size);
        if (size == 0)
        {
            data->callback(std::move(data->buffer), 0);
            data->close_req_used = true;
            int r = uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            if (r != 0)
            {
                // Synchronous error - close failed, cleanup manually
                on_close(&data->close_req);
            }
            return;
        }

        data->iov = uv_buf_init(reinterpret_cast<char*>(data->buffer.data()), static_cast<unsigned int>(size));
        data->read_req_used = true;
        int r = uv_fs_read(LibuvLoop::Get().GetLoop(), &data->read_req, data->open_req.result, &data->iov, 1, 0, on_read);
        if (r != 0)
        {
            // Synchronous error - queue operation failed
            data->callback({}, r);
            if (data->open_req_used)
                uv_fs_req_cleanup(&data->open_req);
            if (data->stat_req_used)
                uv_fs_req_cleanup(&data->stat_req);
            if (data->read_req_used)
                uv_fs_req_cleanup(&data->read_req);
            delete data;
        }
    }

    static void on_open(uv_fs_t* req)
    {
        auto* data = static_cast<AsyncReadData*>(req->data);
        if (req->result < 0)
        {
            data->callback({}, static_cast<int>(req->result));
            if (data->open_req_used)
                uv_fs_req_cleanup(&data->open_req);
            delete data;
            return;
        }

        // Now stat to get size
        data->stat_req_used = true;
        int r = uv_fs_fstat(LibuvLoop::Get().GetLoop(), &data->stat_req, req->result, on_stat);
        if (r != 0)
        {
            // Synchronous error - queue operation failed
            data->callback({}, r);
            if (data->open_req_used)
                uv_fs_req_cleanup(&data->open_req);
            if (data->stat_req_used)
                uv_fs_req_cleanup(&data->stat_req);
            delete data;
        }
    }

    void ReadAllBytesAsync(const std::string& path, std::function<void(std::vector<uint8_t>, int)> callback)
    {
        auto* data = new AsyncReadData(path, std::move(callback));
        data->open_req.data = data;
        data->read_req.data = data;
        data->close_req.data = data;
        data->stat_req.data = data;

        data->open_req_used = true;
        int r = uv_fs_open(LibuvLoop::Get().GetLoop(), &data->open_req, data->path.c_str(), O_RDONLY, 0, on_open);
        if (r != 0)
        {
            // Synchronous error - queue operation failed
            callback({}, r);
            if (data->open_req_used)
                uv_fs_req_cleanup(&data->open_req);
            delete data;
        }
    }
} // namespace OpenRCT2::Platform

#endif