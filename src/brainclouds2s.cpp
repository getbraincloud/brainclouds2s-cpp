#include "brainclouds2s.h"

#include <curl/curl.h>
#include <json/json.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>

#define s2s_log(...) {printf(__VA_ARGS__); fflush(stdout);}

using AuthenticateCallback = std::function<void(const Json::Value&)>;

// Error code for expired session
static const int SERVER_SESSION_EXPIRED = 40365;

// 30 minutes heartbeat interval
static const int HEARTBEAT_INTERVALE_MS = 60 * 30 * 1000;

class S2SContext_internal final 
    : public S2SContext
    , public std::enable_shared_from_this<S2SContext_internal>
{
public:
    S2SContext_internal(const std::string& appId,
                        const std::string& serverName,
                        const std::string& serverSecret,
                        const std::string& url);
    ~S2SContext_internal();

    void setLogEnabled(bool enabled) override;
    void request(
        const std::string& json,
        const S2SCallback& callback) override;
    std::string requestSync(const std::string& json) override;
    void runCallbacks(uint64_t timeoutMS = 0) override;

public: // "private" it's internal to this file only, so keep stuff visible
    struct Callback
    {
        S2SCallback callback;
        std::string data;
    };

    struct Request
    {
        Json::Value json;
        S2SCallback callback;
    };

    void authenticate(const AuthenticateCallback& callback);
    void queueRequest(const std::string& json, const S2SCallback& callback);
    void queueRequestPacket(const Json::Value& json, const S2SCallback& callback);
    void s2sRequest(const Json::Value& json, const S2SCallback& callback);
    void curlSend(const std::string& data, 
                  const S2SCallback& successCallback, 
                  const S2SCallback& errorCallback);
    void queueCallback(const Callback& callback);
    void startHeartbeat();
    void stopHeartbeat();
    void disconnect();
    void sendHeartbeat();
    void processCallbacks();
    void popRequest();
    void doNextRequest();

    std::string m_appId;
    std::string m_serverName;
    std::string m_serverSecret;
    std::string m_url;

    enum State
    {
        Disonnected = 0,
        Authenticating = 1,
        Authenticated = 2
    };

    bool m_logEnabled = false;
    std::atomic<State> m_state;
    int m_packetId = 0;
    std::string m_sessionId = "";

    std::chrono::system_clock::time_point m_heartbeatStartTime;
    std::chrono::milliseconds m_heartbeatInverval;

    // Callbacks queue
    std::mutex m_callbacksMutex;
    std::condition_variable m_callbacksCond;
    std::queue<Callback> m_callbacks;

    std::mutex m_requestsMutex;
    std::vector<Request> m_requestQueue;
};

S2SContextRef S2SContext::create(const std::string& appId,
                                 const std::string& serverName,
                                 const std::string& serverSecret,
                                 const std::string& url)
{
    return S2SContextRef(
        new S2SContext_internal(appId, serverName, serverSecret, url)
    );
}

S2SContext_internal::S2SContext_internal(const std::string& appId,
                                         const std::string& serverName,
                                         const std::string& serverSecret,
                                         const std::string& url)
    : m_appId(appId)
    , m_serverName(serverName)
    , m_serverSecret(serverSecret)
    , m_url(url)
    , m_state(State::Disonnected)
    , m_heartbeatInverval(HEARTBEAT_INTERVALE_MS)
{
}

S2SContext_internal::~S2SContext_internal()
{
    disconnect();
}

void S2SContext_internal::setLogEnabled(bool enabled)
{
    m_logEnabled = enabled;
}

void S2SContext_internal::authenticate(const AuthenticateCallback& callback)
{
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
    queueRequestPacket(json, [pThis, callback](const std::string& dataStr)
    {
        Json::Value data;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(dataStr.c_str(), data);
        if (!parsingSuccessful)
        {
            Json::Value json(Json::ValueType::objectValue);
            json["status"] = 900;
            json["message"] = "Failed to parse json";
            pThis->disconnect();
            callback(json);
            return;
        }

        const auto& messageResponses = data["messageResponses"];
        if (!messageResponses.isNull() && 
            messageResponses.size() > 0 &&
            messageResponses[0]["status"].isInt() &&
            messageResponses[0]["status"].asInt() == 200)
        {
            const auto& message = messageResponses[0];

            pThis->m_state = State::Authenticated;
            pThis->m_packetId = data["packetId"].asInt() + 1;
            const auto& messageData = message["data"];
            pThis->m_sessionId = messageData["sessionId"].asString();
            const auto& heartbeatSeconds = messageData["heartbeatSeconds"];
            if (heartbeatSeconds.isInt())
            {
                pThis->m_heartbeatInverval = 
                    std::chrono::milliseconds(heartbeatSeconds.asInt() * 1000);
            }

            pThis->startHeartbeat();
            callback(message);
        }
        else
        {
            Json::Value json(Json::ValueType::objectValue);
            json["status"] = 900;
            json["message"] = "Malformed json";
            pThis->disconnect();
            callback(json);
        }
    });
}

void S2SContext_internal::queueRequest(
    const std::string& json, const S2SCallback& callback)
{
    // Build packet json
    Json::Value packet(Json::ValueType::objectValue);
    Json::Value messages(Json::ValueType::arrayValue);

    // Parse user json
    {
        Json::Value data;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(json.c_str(), data);
        if (!parsingSuccessful)
        {
            s2s_log("[S2S Error] Failed to parse user json");
            callback("{\"status\":900,\"message\":\"Failed to parse user json\"}");
            return;
        }
        messages.append(data);
    }

    packet["messages"] = messages;

    auto pThis = shared_from_this();
    queueRequestPacket(packet, [pThis, callback](const std::string& dataStr)
    {
        Json::Value data;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(dataStr.c_str(), data);
        if (!parsingSuccessful)
        {
            Json::Value json(Json::ValueType::objectValue);
            json["status"] = 900;
            json["message"] = "Failed to parse json";

            Json::StreamWriterBuilder builder;
            builder["indentation"] = ""; // If you want whitespace-less output
            std::string callback_message = Json::writeString(builder, json);
            callback(callback_message);
            return;
        }

        const auto& messageResponses = data["messageResponses"];
        if (!messageResponses.isNull() && 
            messageResponses.size() > 0 &&
            messageResponses[0]["status"].isInt() &&
            messageResponses[0]["status"].asInt() == 200)
        {
            const auto& message = messageResponses[0];

            Json::StreamWriterBuilder builder;
            builder["indentation"] = ""; // If you want whitespace-less output
            std::string callback_message = Json::writeString(builder, message);
            callback(callback_message);
        }
        else
        {
            Json::Value json(Json::ValueType::objectValue);
            json["status"] = 900;
            json["message"] = "Malformed json";

            Json::StreamWriterBuilder builder;
            builder["indentation"] = ""; // If you want whitespace-less output
            std::string callback_message = Json::writeString(builder, json);
            callback(callback_message);
        }
    });
}

void S2SContext_internal::queueRequestPacket(const Json::Value& json, const S2SCallback& callback)
{
    m_requestsMutex.lock();
    m_requestQueue.push_back({json, callback});

    if (m_requestQueue.size() == 1)
    {
        s2sRequest(json, callback);
    }

    m_requestsMutex.unlock();
}

void S2SContext_internal::popRequest()
{
    std::unique_lock<std::mutex> lock(m_requestsMutex);
    if (m_requestQueue.empty()) return;
    m_requestQueue.erase(m_requestQueue.begin());
}

void S2SContext_internal::doNextRequest()
{
    Request request;
    
    {
        std::unique_lock<std::mutex> lock(m_requestsMutex);
        if (m_requestQueue.empty()) return;
        request = m_requestQueue.front();
    }

    s2sRequest(request.json, request.callback);
}

void S2SContext_internal::s2sRequest(
    const Json::Value& json, const S2SCallback& callback)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // If you want whitespace-less output

    auto packet = json;
    if (m_state == State::Authenticated)
    {
        packet["packetId"] = m_packetId++;
        packet["sessionId"] = m_sessionId;
    }

    std::string postData = Json::writeString(builder, packet);

    if (m_logEnabled)
    {
        s2s_log("[S2S SEND %s] %s\n", m_appId.c_str(), postData.c_str());
    }

    auto pThis = shared_from_this();

    auto popAndDoNextRequest = [pThis, callback](const std::string& data)
    {
        pThis->popRequest();
        if (callback)
        {
            callback(data);
        }
        pThis->doNextRequest();
    };

    curlSend(postData, [pThis, callback, popAndDoNextRequest](const std::string& data)
    {
        if (pThis->m_logEnabled)
        {
            s2s_log("[S2S RECV %s] %s\n", 
                pThis->m_appId.c_str(), data.c_str());
        }
        popAndDoNextRequest(data);

    }, [pThis, callback, popAndDoNextRequest](const std::string& data)
    {
        if (pThis->m_logEnabled)
        {
            s2s_log("[S2S Error %s] %s\n", 
                pThis->m_appId.c_str(), data.c_str());
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
    char * toWrite,
    size_t size,
    size_t nmemb,
    void * data)
{
    std::string* pOutData = (std::string*)data;

    // What we will return
    size_t result = 0;

    // Check for a valid response object.
    if (pOutData != NULL)
    {
        // Append the data to the buffer
        pOutData->append(toWrite, size * nmemb);

        // How much did we write?
        result = size * nmemb;
    }

    return result;
}

void S2SContext_internal::curlSend(const std::string& postData, 
                                   const S2SCallback& successCallback, 
                                   const S2SCallback& errorCallback)
{
    auto pThis = shared_from_this();
    auto sendThread = std::thread(
        [pThis, postData, successCallback, errorCallback]
    {
        CURL* curl = curl_easy_init();

        if (!curl)
        {
            if (errorCallback)
            {
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
        struct curl_slist * headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string contentLength = 
            "Content-Length: " + std::to_string(postData.size());
        headers = curl_slist_append(headers, contentLength.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set up the object to store the content of the response.
        std::string result;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (long)0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (long)0);

        // Set the base URL for the request.
        curl_easy_setopt(curl, CURLOPT_URL, pThis->m_url.c_str());

        // Create all of the form data.
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.size());
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, postData.c_str());

        CURLcode rc = curl_easy_perform(curl);

        if (rc == CURLE_OPERATION_TIMEDOUT)
        {
            if (errorCallback)
            {
                pThis->queueCallback({
                    errorCallback,
                    "{\"status\":900,\"message\":\"Operation timed out\"}"
                });
            }
            return;
        }
        else if (rc != CURLE_OK)
        {
            if (errorCallback)
            {
                pThis->queueCallback({
                    errorCallback,
                    std::string("{\"status\":900,\"message\":\"") + 
                    curlError + "\"}"
                });
            }
            return;
        }

        // Clean up memory.
        if (headers != NULL)
        {
            curl_slist_free_all(headers);
        }
        curl_easy_cleanup(curl);

        if (successCallback)
        {
            pThis->queueCallback({
                successCallback,
                result
            });
        }
    });
    sendThread.detach();
}
                
void S2SContext_internal::request(
    const std::string& json,
    const S2SCallback& callback)
{
    // Authenticate if we are disconnected (Not auth-ed or auth-ing)
    if (m_state == State::Disonnected)
    {
        auto pThis = shared_from_this();
        authenticate([pThis, callback](const Json::Value& data)
        {
            if (data.isNull() || data["status"].asInt() != 200)
            {
                pThis->disconnect();
                callback("{\"status\":900}");
            }
        });
    }

    // Queue request. This will also send the request if the
    // queue is empty
    queueRequest(json, callback);
}

std::string S2SContext_internal::requestSync(const std::string& json)
{
    std::string ret;
    bool processed = false;

    request(json, [&](const std::string& result)
    {
        ret = result;
        processed = true;
    });

    // Timeout after 60sec. This call shouldn't be that long
    auto startTime = std::chrono::steady_clock::now();
    while (!processed && 
           std::chrono::steady_clock::now() < 
           startTime + std::chrono::seconds(60))
    {
        runCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!processed)
    {
        ret = "{\"status\":900,\"message\":\"Request timeout\"}";
    }

    return std::move(ret);
}

void S2SContext_internal::startHeartbeat()
{
    stopHeartbeat();

    m_heartbeatStartTime = std::chrono::system_clock::now();
}

void S2SContext_internal::stopHeartbeat()
{
}

void S2SContext_internal::disconnect()
{
    stopHeartbeat();

    std::unique_lock<std::mutex> lock(m_requestsMutex);
    m_requestQueue.clear();

    m_state = State::Disonnected;
    int m_packetId = 0; // Super important!
    std::string m_sessionId = "";
}

void S2SContext_internal::queueCallback(const Callback& callback)
{
    std::unique_lock<std::mutex> lock(m_callbacksMutex);
    m_callbacks.push(callback);
    m_callbacksCond.notify_all();
}

void S2SContext_internal::sendHeartbeat()
{
    auto pThis = shared_from_this();
    queueRequest("{ \
        \"service\":\"heartbeat\", \
        \"operation\":\"HEARTBEAT\" \
    }", [pThis](const std::string& result)
    {
        Json::Value data;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(result.c_str(), data);
        if (!parsingSuccessful || 
            data.isNull() || 
            data["status"].asInt() != 200)
        {
            pThis->disconnect();
            return;
        }
    });
}

void S2SContext_internal::processCallbacks()
{
    m_callbacksMutex.lock();
    while (!m_callbacks.empty())
    {
        auto callback = m_callbacks.front();
        m_callbacks.pop();
        
        if (callback.callback)
        {
            m_callbacksMutex.unlock();
            callback.callback(callback.data);
            m_callbacksMutex.lock();
        }
    }
    m_callbacksMutex.unlock();
}

void S2SContext_internal::runCallbacks(uint64_t timeoutMS)
{
    // Send heartbeat if we have to
    if (m_state == State::Authenticated)
    {
        auto now = std::chrono::system_clock::now();
        auto timeDiff = now - m_heartbeatStartTime;
        if (timeDiff >= std::chrono::milliseconds(m_heartbeatInverval))
        {
            sendHeartbeat();
            m_heartbeatStartTime = now;
            timeDiff = decltype(timeDiff)::zero();
        }

        // Just wait for the specified timeout
        if (timeoutMS > 0)
        {
            auto waitTime = std::min(
                std::chrono::milliseconds(timeoutMS),
                std::chrono::milliseconds(m_heartbeatInverval) - 
                std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff)
            );
            std::unique_lock<std::mutex> lock(m_callbacksMutex);
            m_callbacksCond.wait_for(lock, waitTime);
        }
    }
    else if (timeoutMS > 0)
    {
        // Just wait for the specified timeout
        std::unique_lock<std::mutex> lock(m_callbacksMutex);
        m_callbacksCond.wait_for(lock, std::chrono::milliseconds(timeoutMS));
    }

    processCallbacks();
}
