// Copyright 2026 bitHeads, Inc. All Rights Reserved.
#pragma once

#include <curl/curl.h>
#include <mutex>
#ifndef _WIN32
#include <csignal>
#endif

// One process-wide curl share handle, so the connection pool and TLS session cache are
// reused across every easy handle this library creates.
//
// Without it each request does a full TCP + TLS handshake, because curl only pools
// connections within a single easy handle (or across handles that share one of these).
// An S2S library is long-lived and chatty, so that churn accumulates source ports in
// TIME_WAIT - ~60s on Linux, ~120s on Windows - and starts failing to connect against
// single-IP endpoints where every connection competes for the same 4-tuple space.
//
// Mirrors the fix already in the C++ client SDK (src/nix/cURLLoader.cpp, May 2026).
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

    // Built once, on first use. Function-local statics are thread-safe since C++11, which
    // matters here because requests are issued from worker threads.
    inline CURLSH* s2sCurlShare()
    {
        static CURLSH* share = []() -> CURLSH*
        {
            curl_global_init(CURL_GLOBAL_ALL);
#ifndef _WIN32
            // SIGPIPE is raised when writing to a socket whose remote end has closed.
            // CURLOPT_NOSIGNAL suppresses SIGALRM only, not SIGPIPE. With pooling, a
            // cached connection can be dead (server closed it after an idle period) and
            // curl's attempt to write to it would kill the process. Ignoring it lets curl
            // see EPIPE instead, drop the dead connection and retry on a fresh one.
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
