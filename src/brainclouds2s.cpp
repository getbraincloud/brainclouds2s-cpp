#include "brainclouds2s.h"
#include "brainclouds2s-rtt.h"
#include "RTTComms.h"
#include <curl/curl.h>
#include <json/json.h>


#include <chrono>
#include <limits>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <sstream>
#include <iomanip>
#include <cctype>
#include "stdlib.h"


using AuthenticateCallback = std::function<void(const Json::Value&)>;

namespace BrainCloud {
// Error code for expired session
    static const int SERVER_SESSION_EXPIRED = 40365;

// 30 minutes heartbeat interval
    static const int HEARTBEAT_INTERVALE_MS = 60 * 30 * 1000;

    static std::string toString(const Json::Value &json) {
        Json::FastWriter writer;
        return writer.write(json);
    }

    std::string g_logFilePath;
    bool g_showSecretLogs = false;

    class S2SContext_internal final
            : public S2SContext, public std::enable_shared_from_this<S2SContext_internal> {
    public:
        S2SContext_internal(const std::string &appId,
                            const std::string &serverName,
                            const std::string &serverSecret,
                            const std::string &url,
                            bool autoAuth);

        ~S2SContext_internal();

        void setLogEnabled(bool enabled) override;

        BrainCloudRTT* getRTTService() override;

        void authenticate(const S2SCallback &callback) override;

        std::string authenticateSync() override;

        void enableRTT(IRTTConnectCallback* callback) override;

        void request(
                const std::string &json,
                const S2SCallback &callback) override;

        std::string requestSync(const std::string &json) override;

        void runCallbacks(uint64_t timeoutMS = 0) override;

    public: // "private" it's internal to this file only, so keep stuff visible
        struct Callback {
            S2SCallback callback;
            std::string data;
        };

        struct Request {
            Json::Value json;
            S2SCallback callback;
        };

        void authenticateInternal(const AuthenticateCallback &callback);

        void onAuthenticateResult(const Json::Value &json, const S2SCallback &callback);

        void queueRequest(const std::string &json, const S2SCallback &callback);

        void queueRequestPacket(const Json::Value &json, const S2SCallback &callback);

        void s2sRequest(const Json::Value &json, const S2SCallback &callback);

        void curlSend(const std::string &data,
                      const S2SCallback &successCallback,
                      const S2SCallback &errorCallback);

        void queueCallback(const Callback &callback);

        void startHeartbeat();

        void stopHeartbeat();

        void disconnect();

        void sendHeartbeat();

        void processCallbacks();

        void popRequest();

        void doNextRequest();

        bool m_autoAuth = false;

        enum State {
            Disconnected = 0,
            Authenticating = 1,
            Authenticated = 2
        };

        bool m_logEnabled = false;
        std::atomic <State> m_state;
        int m_packetId = 0;

        std::chrono::system_clock::time_point m_heartbeatStartTime;
        std::chrono::milliseconds m_heartbeatInverval;

        // Callbacks queue
        std::mutex m_callbacksMutex;
        std::condition_variable m_callbacksCond;
        std::queue <Callback> m_callbacks;

        std::mutex m_requestsMutex;
        std::vector <Request> m_requestQueue;



        // RTT
        RTTComms * m_rttComms;
        BrainCloudRTT * m_rttService;
    };

    S2SContextRef S2SContext::create(const std::string &appId,
                                     const std::string &serverName,
                                     const std::string &serverSecret,
                                     const std::string &url,
                                     bool autoAuth) {
        return S2SContextRef(
                new S2SContext_internal(appId, serverName, serverSecret, url, autoAuth)
        );
    }

    

    S2SContext_internal::S2SContext_internal(const std::string &appId,
                                             const std::string &serverName,
                                             const std::string &serverSecret,
                                             const std::string &url,
                                             bool autoAuth)
            : m_state(State::Disconnected), m_heartbeatInverval(HEARTBEAT_INTERVALE_MS), m_autoAuth(autoAuth),
              m_rttComms(new RTTComms(this))
{
        m_appId = appId;
        m_serverName = serverName;
        m_serverSecret = serverSecret;
        m_url = url;
        m_rttService = new BrainCloudRTT(m_rttComms, this);
        if (m_rttComms)
        {
            m_rttComms->resetCommunication();
            m_rttComms->initialize();
        }
    }

    S2SContext_internal::~S2SContext_internal() {
        delete m_rttService;
        delete m_rttComms;
        disconnect();
    }

    BrainCloudRTT* S2SContext_internal::getRTTService() {
        return m_rttService;
    }

    void S2SContext_internal::setLogEnabled(bool enabled) {
        m_logEnabled = enabled;
        m_rttComms->enableLogging(enabled);
    }

    void S2SContext_internal::authenticateInternal(const AuthenticateCallback &callback) {
        // Build the authentication json
        Json::Value json(Json::ValueType::objectValue);
        json["packetId"] = 0;
        Json::Value messages(Json::ValueType::arrayValue);
        Json::Value message(Json::ValueType::objectValue);
        message["service"] = "authenticationV2";
        message["operation"] = "AUTHENTICATE";
        Json::Value data(Json::ValueType::objectValue);
        data["appId"] = m_appId;
        data["serverName"] = m_serverName;
        data["serverSecret"] = m_serverSecret;
        message["data"] = data;
        messages.append(message);
        json["messages"] = messages;

        m_state = State::Authenticating;
        auto pThis = shared_from_this();
        queueRequestPacket(json, [pThis, callback](const std::string &dataStr) {
            Json::Value data;
            Json::Reader reader;
            bool parsingSuccessful = reader.parse(dataStr.c_str(), data);
            if (!parsingSuccessful) {
                Json::Value json(Json::ValueType::objectValue);
                json["status"] = 900;
                json["message"] = "Failed to parse json";
                callback(json);
                return;
            }

            const auto &messageResponses = data["messageResponses"];
            if (!messageResponses.isNull() &&
                messageResponses.size() > 0 &&
                messageResponses[0]["status"].isInt() &&
                messageResponses[0]["status"].asInt() == 200) {
                const auto &message = messageResponses[0];

                pThis->m_state = State::Authenticated;
                pThis->m_packetId = data["packetId"].asInt() + 1;
                const auto &messageData = message["data"];
                pThis->m_sessionId = messageData["sessionId"].asString();
                const auto &heartbeatSeconds = messageData["heartbeatSeconds"];
                if (heartbeatSeconds.isInt()) {
                    pThis->m_heartbeatInverval =
                            std::chrono::milliseconds(heartbeatSeconds.asInt() * 1000);
                }

                pThis->startHeartbeat();
                callback(message);
            } else {
                Json::Value json(Json::ValueType::objectValue);
                json["status"] = 900;
                json["message"] = "Malformed json";
                callback(json);
            }

            s2s_log("Session ID:", pThis->m_sessionId);
        });
    }

    void S2SContext_internal::queueRequest(
            const std::string &json, const S2SCallback &callback) {
        // Build packet json
        Json::Value packet(Json::ValueType::objectValue);
        Json::Value messages(Json::ValueType::arrayValue);

        // Parse user json
        {
            Json::Value data;
            Json::Reader reader;
            bool parsingSuccessful = reader.parse(json.c_str(), data);
            if (!parsingSuccessful) {
                s2s_log("[S2S Error] Failed to parse user json");
                if (callback) {
                    callback("{\"status\":900,\"message\":\"Failed to parse user json\"}");
                }
                return;
            }
            messages.append(data);
        }

        packet["messages"] = messages;

        auto pThis = shared_from_this();
        queueRequestPacket(packet, [pThis, callback](const std::string &dataStr) {
            Json::Value data;
            Json::Reader reader;
            bool parsingSuccessful = reader.parse(dataStr.c_str(), data);
            if (!parsingSuccessful) {
                Json::Value json(Json::ValueType::objectValue);
                json["status"] = 900;
                json["message"] = "Failed to parse json";

                if (callback) {
                    std::string callback_message = toString(json);
                    callback(callback_message);
                }
                return;
            }

            const auto &messageResponses = data["messageResponses"];
            if (!messageResponses.isNull() &&
                messageResponses.size() > 0 &&
                messageResponses[0]["status"].isInt() &&
                messageResponses[0]["status"].asInt() == 200) {
                const auto &message = messageResponses[0];

                if (callback) {
                    std::string callback_message = toString(message);
                    callback(callback_message);
                }
            } else {
                Json::Value json(Json::ValueType::objectValue);
                json["status"] = 900;
                json["message"] = "Malformed json";

                if (callback) {
                    std::string callback_message = toString(json);
                    callback(callback_message);
                }
            }
        });
    }

    void S2SContext_internal::queueRequestPacket(const Json::Value &json, const S2SCallback &callback) {
        m_requestsMutex.lock();
        m_requestQueue.push_back({json, callback});

        if (m_requestQueue.size() == 1) {
            s2sRequest(json, callback);
        }

        m_requestsMutex.unlock();
    }

    void S2SContext_internal::popRequest() {
        std::unique_lock <std::mutex> lock(m_requestsMutex);
        if (m_requestQueue.empty()) return;
        m_requestQueue.erase(m_requestQueue.begin());
    }

    void S2SContext_internal::doNextRequest() {
        Request request;

        {
            std::unique_lock <std::mutex> lock(m_requestsMutex);
            if (m_requestQueue.empty()) return;
            request = m_requestQueue.front();
        }

        s2sRequest(request.json, request.callback);
    }

    void S2SContext_internal::s2sRequest(
            const Json::Value &json, const S2SCallback &callback) {
        auto packet = json;
        if (m_state == State::Authenticated) {
            packet["packetId"] = m_packetId++;
            packet["sessionId"] = m_sessionId;
        }

        std::string postData = toString(packet);
        rtrim(postData);

        if (m_logEnabled) {
            s2s_log("[S2S SEND ", m_appId.c_str(), "] ", postData);
        }

        auto pThis = shared_from_this();

        auto popAndDoNextRequest = [pThis, callback](const std::string &data) {
            if (!pThis) {
                std::cerr << "pThis is null" << std::endl;
                return;
            }
            pThis->popRequest();
            if (callback) {
                callback(data);
            }
            pThis->doNextRequest();
        };

        curlSend(postData, [pThis, popAndDoNextRequest](const std::string &data) {
            if (pThis->m_logEnabled) {
                s2s_log("[S2S RECV ", pThis->m_appId, "] ", data);
            }
            popAndDoNextRequest(data);

        }, [pThis, popAndDoNextRequest](const std::string &data) {
            if (pThis->m_logEnabled) {
                s2s_log("[S2S Error ", pThis->m_appId, "] ", data);
            }
            popAndDoNextRequest(data);
        });
    }

/**
 * This is the writer call back function used by curl
 *
 * @param toWrite - data received from the remote server
 * @param size - size of a character (?)
 * @param nmemb - number of characters
 * @param data - pointer to the curlloader
 *
 * @return int - number of characters received (should equal size * nmemb)
 */
    static size_t writeData(
            char *toWrite,
            size_t size,
            size_t nmemb,
            void *data) {
        std::string *pOutData = (std::string *) data;

        // What we will return
        size_t result = 0;

        // Check for a valid response object.
        if (pOutData != NULL) {
            // Append the data to the buffer
            pOutData->append(toWrite, size * nmemb);

            // How much did we write?
            result = size * nmemb;
        }

        return result;
    }

    void S2SContext_internal::curlSend(const std::string &postData,
                                       const S2SCallback &successCallback,
                                       const S2SCallback &errorCallback) {
        auto pThis = shared_from_this();
        auto sendThread = std::thread(
                [pThis, postData, successCallback, errorCallback] {
                    CURL *curl = curl_easy_init();

                    if (!curl) {
                        if (errorCallback) {
                            pThis->queueCallback({
                                                         errorCallback,
                                                         "{\"status\":900,\"message\":\"cURL Out of Memory\"}"
                                                 });
                        }
                        return;
                    }

                    char curlError[CURL_ERROR_SIZE];

                    // Use an error buffer to store the description of any errors.
                    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

                    // Set the headers.
                    struct curl_slist *headers = NULL;
                    headers = curl_slist_append(headers, "Content-Type: application/json");
                    std::string contentLength =
                            "Content-Length: " + std::to_string(postData.size());
                    headers = curl_slist_append(headers, contentLength.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                    // Set up the object to store the content of the response.
                    std::string result;
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);

                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (long) 0);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (long) 0);

                    // Set the base URL for the request.
                    curl_easy_setopt(curl, CURLOPT_URL, pThis->m_url.c_str());

                    // Create all of the form data.
                    curl_easy_setopt(curl, CURLOPT_POST, 1);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.size());
                    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, postData.c_str());

                    CURLcode rc = curl_easy_perform(curl);

                    if (rc == CURLE_OPERATION_TIMEDOUT) {
                        if (errorCallback) {
                            pThis->queueCallback({
                                                         errorCallback,
                                                         "{\"status\":900,\"message\":\"Operation timed out\"}"
                                                 });
                        }
                        return;
                    } else if (rc != CURLE_OK) {
                        if (errorCallback) {
                            pThis->queueCallback({
                                                         errorCallback,
                                                         std::string("{\"status\":900,\"message\":\"") +
                                                         curlError + "\"}"
                                                 });
                        }
                        return;
                    }

                    // Clean up memory.
                    if (headers != NULL) {
                        curl_slist_free_all(headers);
                    }
                    curl_easy_cleanup(curl);

                    if (successCallback) {
                        pThis->queueCallback({
                                                     successCallback,
                                                     result
                                             });
                    }
                });
        sendThread.detach();
    }

    void S2SContext_internal::onAuthenticateResult(const Json::Value &json,
                                                   const S2SCallback &callback) {
        std::string callback_message = toString(json);

        if (json["status"].asInt() != 200) {
            // Create a copy of our previous requests, we will callback
            // then all on failed auth. The reasons are:
            // 1. disconnect() clears the queue
            // 2. A callback might create new requests and cause side
            //    effects so it's better to be in a clean state.
            decltype(m_requestQueue) requestQueueCopy;
            {
                m_requestsMutex.lock();
                requestQueueCopy = m_requestQueue;
                m_requestsMutex.unlock();
            }

            // Disconnect (Clear internal queued requests)
            disconnect();

            // Callback to everyone that were queued
            if (callback) {
                callback(callback_message);
            }
            for (size_t i = m_autoAuth ? 1 : 0 /* On Auto auth, we skip first request, it's the auth itself */;
                 i < requestQueueCopy.size(); i++) {
                const auto &request = requestQueueCopy[i];
                if (request.callback) {
                    request.callback(callback_message);
                }
            }
        } else if (!m_autoAuth) // If we are auto-auth, we don't callback for auth.
        {
            if (callback) {
                callback(callback_message);
            }
        }
    }

    void S2SContext_internal::authenticate(const S2SCallback &callback) {
        if (m_state != State::Disconnected) {
            callback("{\"status\":400,\"message\":\"Already authenticated or authenticating\"}");
            return;
        }

        auto pThis = shared_from_this();
        authenticateInternal([pThis, callback](const Json::Value &data) {
            pThis->onAuthenticateResult(data, callback);
        });
    }

    std::string S2SContext_internal::authenticateSync() {
        std::string ret;
        bool processed = false;

        authenticate([&](const std::string &result) {
            ret = result;
            processed = true;
        });

        // Timeout after 60sec. This call shouldn't be that long
        auto startTime = std::chrono::steady_clock::now();
        while (!processed &&
               std::chrono::steady_clock::now() <
               startTime + std::chrono::seconds(60)) {
            runCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!processed) {
            ret = "{\"status\":900,\"message\":\"Authenticate timeout\"}";
        }

        return std::move(ret);
    }


    void S2SContext_internal::enableRTT(IRTTConnectCallback *callback) {
        auto pThis = shared_from_this();
        m_rttService->enableRTT(callback, true);
    }

    void S2SContext_internal::request(
            const std::string &json,
            const S2SCallback &callback) {
        // Authenticate if we are disconnected (Not auth-ed or auth-ing)
        if (m_state == State::Disconnected && m_autoAuth) {
            auto pThis = shared_from_this();
            authenticateInternal([pThis, callback](const Json::Value &data) {
                pThis->onAuthenticateResult(data, callback);
            });
        }

        // Queue request. This will also send the request if the
        // queue is empty
        queueRequest(json, callback);
    }

    std::string S2SContext_internal::requestSync(const std::string &json) {
        std::string ret;
        bool processed = false;

        request(json, [&](const std::string &result) {
            ret = result;
            processed = true;
        });

        // Timeout after 60sec. This call shouldn't be that long
        auto startTime = std::chrono::steady_clock::now();
        while (!processed &&
               std::chrono::steady_clock::now() <
               startTime + std::chrono::seconds(60)) {
            runCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!processed) {
            ret = "{\"status\":900,\"message\":\"Request timeout\"}";
        }

        return std::move(ret);
    }

    void S2SContext_internal::startHeartbeat() {
        stopHeartbeat();

        m_heartbeatStartTime = std::chrono::system_clock::now();
    }

    void S2SContext_internal::stopHeartbeat() {
    }

    void S2SContext_internal::disconnect() {
        stopHeartbeat();

        m_requestsMutex.lock();
        m_requestQueue.clear();
        m_requestsMutex.unlock();

        m_state = State::Disconnected;
        int m_packetId = 0; // Super important!
        std::string m_sessionId = "";
    }

    void S2SContext_internal::queueCallback(const Callback &callback) {
        std::unique_lock <std::mutex> lock(m_callbacksMutex);
        m_callbacks.push(callback);
        m_callbacksCond.notify_all();
    }

    void S2SContext_internal::sendHeartbeat() {
        auto pThis = shared_from_this();
        queueRequest("{ \
        \"service\":\"heartbeat\", \
        \"operation\":\"HEARTBEAT\" \
    }", [pThis](const std::string &result) {
            Json::Value data;
            Json::Reader reader;
            bool parsingSuccessful = reader.parse(result.c_str(), data);
            if (!parsingSuccessful ||
                data.isNull() ||
                data["status"].asInt() != 200) {
                pThis->disconnect();
                return;
            }
        });
    }

    void S2SContext_internal::processCallbacks() {
        m_callbacksMutex.lock();
        while (!m_callbacks.empty()) {
            auto callback = m_callbacks.front();
            m_callbacks.pop();

            if (callback.callback) {
                m_callbacksMutex.unlock();
                callback.callback(callback.data);
                m_callbacksMutex.lock();
            }
        }
        m_callbacksMutex.unlock();
    }

    void S2SContext_internal::runCallbacks(uint64_t timeoutMS) {
        // Send heartbeat if we have to
        if (m_state == State::Authenticated) {
            auto now = std::chrono::system_clock::now();
            auto timeDiff = now - m_heartbeatStartTime;
            if (timeDiff >= std::chrono::milliseconds(m_heartbeatInverval)) {
                sendHeartbeat();
                m_heartbeatStartTime = now;
            }

            // Just wait for the specified timeout
            if (timeoutMS > 0) {
                auto hbIntervalDuration = std::chrono::milliseconds(m_heartbeatInverval) -
                    std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff);
                auto timeoutDuration = std::chrono::milliseconds(timeoutMS);
                auto waitTime = timeoutDuration < hbIntervalDuration ? timeoutDuration : hbIntervalDuration;
                std::unique_lock <std::mutex> lock(m_callbacksMutex);
                m_callbacksCond.wait_for(lock, waitTime);
            }
        } else if (timeoutMS > 0) {
            // Just wait for the specified timeout
            std::unique_lock <std::mutex> lock(m_callbacksMutex);
            m_callbacksCond.wait_for(lock, std::chrono::milliseconds(timeoutMS));
        }

        processCallbacks();

        m_rttComms->runCallbacks();
    }

    
    void logToFile(const std::string& path)
    {
        g_logFilePath = path;
    }
}