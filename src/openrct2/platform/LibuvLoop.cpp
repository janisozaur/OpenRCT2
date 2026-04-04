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

        AsyncReadData(const std::string& p, std::function<void(std::vector<uint8_t>, int)> cb)
            : path(p)
            , callback(std::move(cb))
        {
        }
    };

    static void on_read(uv_fs_t* req);

    static void on_close(uv_fs_t* req)
    {
        auto* data = static_cast<AsyncReadData*>(req->data);
        uv_fs_req_cleanup(&data->open_req);
        uv_fs_req_cleanup(&data->stat_req);
        uv_fs_req_cleanup(&data->read_req);
        uv_fs_req_cleanup(&data->close_req);
        delete data;
    }

    static void on_read(uv_fs_t* req)
    {
        auto* data = static_cast<AsyncReadData*>(req->data);
        if (req->result < 0)
        {
            data->callback({}, static_cast<int>(req->result));
            uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
        }
        else if (req->result == 0)
        {
            // End of file
            data->callback(std::move(data->buffer), 0);
            uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
        }
        else
        {
            data->offset += req->result;
            if (data->offset < data->buffer.size())
            {
                data->iov = uv_buf_init(
                    reinterpret_cast<char*>(data->buffer.data() + data->offset),
                    static_cast<unsigned int>(data->buffer.size() - data->offset));
                uv_fs_read(
                    LibuvLoop::Get().GetLoop(), &data->read_req, data->open_req.result, &data->iov, 1, data->offset, on_read);
            }
            else
            {
                data->callback(std::move(data->buffer), 0);
                uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            }
        }
    }

    static void on_stat(uv_fs_t* stat_req)
    {
        auto* data = static_cast<AsyncReadData*>(stat_req->data);
        if (stat_req->result < 0)
        {
            data->callback({}, static_cast<int>(stat_req->result));
            uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            return;
        }

        uint64_t size = stat_req->statbuf.st_size;
        data->buffer.resize(size);
        if (size == 0)
        {
            data->callback(std::move(data->buffer), 0);
            uv_fs_close(LibuvLoop::Get().GetLoop(), &data->close_req, data->open_req.result, on_close);
            return;
        }

        data->iov = uv_buf_init(reinterpret_cast<char*>(data->buffer.data()), static_cast<unsigned int>(size));
        uv_fs_read(LibuvLoop::Get().GetLoop(), &data->read_req, data->open_req.result, &data->iov, 1, 0, on_read);
    }

    static void on_open(uv_fs_t* req)
    {
        auto* data = static_cast<AsyncReadData*>(req->data);
        if (req->result < 0)
        {
            data->callback({}, static_cast<int>(req->result));
            uv_fs_req_cleanup(&data->open_req);
            delete data;
            return;
        }

        // Now stat to get size
        uv_fs_fstat(LibuvLoop::Get().GetLoop(), &data->stat_req, req->result, on_stat);
    }

    void ReadAllBytesAsync(const std::string& path, std::function<void(std::vector<uint8_t>, int)> callback)
    {
        auto* data = new AsyncReadData(path, std::move(callback));
        data->open_req.data = data;
        data->read_req.data = data;
        data->close_req.data = data;
        data->stat_req.data = data;

        uv_fs_open(LibuvLoop::Get().GetLoop(), &data->open_req, data->path.c_str(), O_RDONLY, 0, on_open);
    }
} // namespace OpenRCT2::Platform

#endif
