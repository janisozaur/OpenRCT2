/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_HTTP

    #include "Http.h"

    #include <algorithm>
    #include <chrono>
    #include <future>
    #include <mutex>
    #include <vector>

namespace OpenRCT2::Http
{
    static std::mutex _asyncRequestsMutex;
    static std::vector<std::shared_future<void>> _asyncRequests;

    std::shared_future<void> DoAsync(const Request& req, std::function<void(Response& res)> fn)
    {
        std::lock_guard<std::mutex> lock(_asyncRequestsMutex);

        // Clean up finished requests
        _asyncRequests.erase(
            std::remove_if(
                _asyncRequests.begin(), _asyncRequests.end(),
                [](const std::shared_future<void>& f) {
                    return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }),
            _asyncRequests.end());

        auto future = std::async(std::launch::async, [=]() {
                          Response res{};
                          try
                          {
                              res = Do(req);
                          }
                          catch (const std::exception& e)
                          {
                              res.status = Status::Error;
                              res.error = e.what();
                          }
                          fn(res);
                      }).share();

        _asyncRequests.push_back(future);
        return future;
    }
} // namespace OpenRCT2::Http

#endif // DISABLE_HTTP
