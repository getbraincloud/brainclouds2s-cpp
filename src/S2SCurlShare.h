// Copyright 2026 bitHeads, Inc. All Rights Reserved.
#pragma once

#include <curl/curl.h>
#include <mutex>
#ifndef _WIN32
#include <csignal>
#endif

// One process-wide curl share handle, so the connection pool and TLS session cache are
// reused across every easy handle this library creates.

namespace BrainCloud
{
    namespace detail
    {
        inline std::mutex* s2sShareMutexes()
        {
            static std::mutex mutexes[CURL_LOCK_DATA_LAST];
            return mutexes;
        }

        inline void s2sShareLock(CURL*, curl_lock_data data, curl_lock_access, void*)
        {
            if (data >= 0 && data < CURL_LOCK_DATA_LAST)
                s2sShareMutexes()[data].lock();
        }

        inline void s2sShareUnlock(CURL*, curl_lock_data data, void*)
        {
            if (data >= 0 && data < CURL_LOCK_DATA_LAST)
                s2sShareMutexes()[data].unlock();
        }
    }

    inline CURLSH* s2sCurlShare()
    {
        static CURLSH* share = []() -> CURLSH*
        {
            curl_global_init(CURL_GLOBAL_ALL);
#ifndef _WIN32
            std::signal(SIGPIPE, SIG_IGN);
#endif
            CURLSH* handle = curl_share_init();
            if (handle == nullptr)
                return nullptr;

            curl_share_setopt(handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
            curl_share_setopt(handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
            curl_share_setopt(handle, CURLSHOPT_LOCKFUNC, detail::s2sShareLock);
            curl_share_setopt(handle, CURLSHOPT_UNLOCKFUNC, detail::s2sShareUnlock);
            return handle;
        }();
        return share;
    }
}
