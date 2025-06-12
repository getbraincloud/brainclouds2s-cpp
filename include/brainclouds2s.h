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

namespace BrainCloud {
    static const std::string DEFAULT_S2S_URL =
            "https://api.braincloudservers.com/s2sdispatcher";

    static const std::string s_brainCloudS2SVersion = "5.7.0";

    class S2SContext;
    class BrainCloudRTT;
    class IRTTConnectCallback;
    using S2SCallback = std::function<void(const std::string &)>;
    using S2SContextRef = std::shared_ptr<S2SContext>;

    void s2s_log(const std::stringstream &message, bool file = false);
    void s2s_log(const std::string &message, bool file = false);

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
