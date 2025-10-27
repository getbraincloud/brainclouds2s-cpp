/*
    2019 @ bitheads inc.
    Author: David St-Louis
*/

#ifndef BRAINCLOUDS2S_H_INCLUDED
#define BRAINCLOUDS2S_H_INCLUDED

#include <functional>
#include <memory>
#include <string>
#include "brainclouds2s-rtt.h"
#include <IRTTConnectCallback.h>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <TimeUtil.h>

#include <regex>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fstream>
#endif

namespace BrainCloud {
    static const std::string DEFAULT_S2S_URL =
            "https://api.braincloudservers.com/s2sdispatcher";

    static const std::string s_brainCloudS2SVersion = "5.8.0";

    class S2SContext;
    class BrainCloudRTT;
    class IRTTConnectCallback;
    using S2SCallback = std::function<void(const std::string &)>;
    using S2SContextRef = std::shared_ptr<S2SContext>;

    static const std::vector<std::string> sensitiveKeys = 
    {
            "secretKey", "serverSecret", "ApiKey", "secret", "token", "X-RTT-SECRET"
    };

    extern std::string g_logFilePath;

    static std::string obfuscateString(const std::string& s) 
    {
        // TODO: explore different methods of obfuscating the string for now just return "[REDACTED]
        return "[REDACTED]";
    }

    static std::string redactSecretKeys(const std::string& input) 
    {
        std::string out = input;

        for (auto& key : sensitiveKeys) {
            size_t pos = 0;
            std::string needle = "\"" + key + "\"";
            while ((pos = out.find(needle, pos)) != std::string::npos) {
                // find the colon after the key
                size_t colon = out.find(':', pos + needle.size());
                if (colon == std::string::npos) break;

                // find the first quote after colon
                size_t quoteStart = out.find('"', colon);
                if (quoteStart == std::string::npos) break;

                // find the closing quote
                size_t quoteEnd = out.find('"', quoteStart + 1);
                if (quoteEnd == std::string::npos) break;

                // replace value
                std::string value = out.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                std::string replacement = obfuscateString(value);
                out.replace(quoteStart + 1, value.size(), replacement);

                pos = quoteEnd + 1;
            }
        }

        return out;
    }

    template<typename ...Args>
    std::string buildLogMessage(Args && ...args)
    {
        std::string timeStamp = TimeUtil::currentTimestamp();
        std::ostringstream oss;
        oss << "[" << timeStamp << "] ";

        using expander = int[];
        (void)expander {
            0, (
                void(
                    // For std::string arguments, redact secrets
                    (std::is_same<typename std::decay<Args>::type, std::string>::value
                        ? oss << redactSecretKeys(std::forward<Args>(args))
                        : oss << args)
                    ), 0)...
        };

        return oss.str();
    }

    template <typename... Args>
    void s2s_log(Args&&... args)
    {
        std::string text = buildLogMessage(std::forward<Args>(args)...);

        if (!g_logFilePath.empty()) {
#if defined(_WIN32)
            // Windows needs this implementation to be able to share the ability to write to this file with another program
            HANDLE hFile = CreateFileA(
                g_logFilePath.c_str(),
                FILE_APPEND_DATA,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                NULL,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE) {
                std::cerr << "Failed to open log file: " << g_logFilePath << std::endl;
                return;
            }

            std::string line = text + "\r\n";
            DWORD written;
            BOOL ok = WriteFile(
                hFile,
                line.c_str(),
                static_cast<DWORD>(line.size()),
                &written,
                NULL
            );

            CloseHandle(hFile);

            if (!ok || written != line.size()) {
                std::cerr << "Failed to write to log file: " << g_logFilePath << std::endl;
                //print to console if failed to write log to file
                std::cout << text << "\n";
            }

#else
            // --- Linux / macOS implementation ---
            std::ofstream out(g_logFilePath.c_str(), std::ios::app);
            if (!out) {
                std::cerr << "Failed to open log file: " << g_logFilePath << std::endl;
                //print to console if failed to write log to file
                std::cout << text << std::endl << std::flush;
                return;
            }
            out << text << "\n";
#endif
        }
        else {
            //Print to console if no file output was set
            std::cout << text << "\n";
        }
    }

    
    /*
    * Set a log file path - if set then logs will be written to this file
    * @param path the file path for the log file
    */
    void logToFile(const std::string& path);

    class S2SContext {
    public:
        /*
         * Create a new S2S context
         * @param appId Application ID
         * @param serverName Server name
         * @param serverSecret Server secret key
         * @param url The server url to send the request to. You can use
         *            DEFAULT_S2S_URL for the default brainCloud servers.
         * @param autoAuth If sets to true, the context will authenticate on the
         *                 first request if it's not already. Otherwise,
         *                 authenticate() or authenticateSync() must be called
         *                 successfully first before doing requests. WARNING: This
         *                 used to be implied true.
         *
         *                 It is recommended to put this to false, manually
         *                 call authenticate, and wait for a successful response
         *                 before proceeding with other requests.
         * @return A new S2S context, or nullptr if something bad happened.
         */
        static S2SContextRef create(const std::string &appId,
                                    const std::string &serverName,
                                    const std::string &serverSecret,
                                    const std::string &url,
                                    bool autoAuth);

        virtual ~S2SContext() {}

        /*
         * Set whether S2S messages and errors are logged to the console
         * @param enabled Will log if true. Default false
         */
        virtual void setLogEnabled(bool enabled) = 0;

        /*
         * Authenticate with brainCloud. If autoAuth is set to false, which is
         * the default, this must be called successfully before doing other
         * requests. See S2SContext::create
         * @param callback Callback function
         */
        virtual void authenticate(const S2SCallback &callback) = 0;

        /*
         * Same as authenticate, but waits for result. This call is blocking.
         * @return Authenticate result
         */
        virtual std::string authenticateSync() = 0;

        /*
         * enable rtt
         */
        virtual void enableRTT(IRTTConnectCallback* callback) = 0;

        /*
         * Send an S2S request.
         * @param json Content to be sent
         * @param callback Callback function
         */
        virtual void request(
                const std::string &json,
                const S2SCallback &callback) = 0;

        /*
         * Send an S2S request, and wait for result. This call is blocking.
         * @param json Content to be sent
         * @return Request result
         */
        virtual std::string requestSync(const std::string &json) = 0;

        /*
         * Update requests and perform callbacks on the calling thread.
         * @param timeoutMS Time to block on the call in milliseconds.
         *                  Pass 0 to return immediately.
         */
        virtual void runCallbacks(uint64_t timeoutMS = 0) = 0;

        virtual BrainCloudRTT* getRTTService() {return nullptr;}

        const std::string& getAppId() const {return m_appId;}
        const std::string& getServerName() const {return m_serverName;}
        const std::string& getServerSecret() const {return m_serverSecret;}
        const std::string& getServerUrl() const {return m_url;}
        const std::string& getSessionId() const {return m_sessionId;}

        const std::string& getS2SVersion() const {return s_brainCloudS2SVersion;}

    protected:
        S2SContext() {}

        std::string m_appId = "";
        std::string m_serverName = "";
        std::string m_serverSecret = "";
        std::string m_url = "";
        std::string m_sessionId = "";
    };
    
}
#endif /* BRAINCLOUDS2S_H_INCLUDED */
