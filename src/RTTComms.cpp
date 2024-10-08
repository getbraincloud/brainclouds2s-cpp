#include "RTTComms.h"
#include "brainclouds2s.h"
#include "IRTTCallback.h"
#include "IRTTConnectCallback.h"
#include "TimeUtil.h"

#include <fstream>
#include <iostream>
#include <thread>
#include <sstream>

#define RTTCOMMS_LOG_EVERY_METHODS 0

namespace BrainCloud
{
    RTTComms::RTTCallback::RTTCallback(RTTCallbackType type)
        : _type(type)
    {
    }

    RTTComms::RTTCallback::RTTCallback(RTTCallbackType type, const std::string& message)
        : _type(type)
        , _message(message)
    {
    }

    RTTComms::RTTCallback::RTTCallback(RTTCallbackType type, const Json::Value& json)
        : _type(type)
        , _json(json)
    {
    }

    RTTComms::RTTCallback::RTTCallback(RTTCallbackType type, const Json::Value& json, const std::string& message)
        : _type(type)
        , _message(message)
        , _json(json)
    {
    }

    RTTComms::RTTComms(S2SContext* c)
        : _isInitialized(false)
        , _context(c)
        , _loggingEnabled(false)
        , _connectCallback(NULL)
        , _socket(NULL)
        , _rttConnectionStatus(BrainCloudRTT::RTTConnectionStatus::Disconnected)
        , _receivingRunning(false)
        , _heartbeatRunning(false)
        , _useWebSocket(true)
        , _heartbeatSeconds(30)
        , _lastHeartbeatTime(0)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::RTTComms");
#endif
    }

    RTTComms::~RTTComms()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::~RTTComms");
#endif
        shutdown();
    }

    void RTTComms::initialize()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::initialize");
#endif
        _isInitialized = true;
    }

    bool RTTComms::isInitialized() const
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::isInitialized");
#endif
        return _isInitialized;
    }

    void RTTComms::shutdown()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::shutdown");
#endif
        resetCommunication();
        _isInitialized = false;
    }

    void RTTComms::resetCommunication()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::resetCommunication");
#endif
        if (isRTTEnabled())
        {
            _rttConnectionStatus = BrainCloudRTT::RTTConnectionStatus::Disconnecting;
            closeSocket();
            _eventQueueMutex.lock();
            _callbackEventQueue.clear();
            _eventQueueMutex.unlock();
            _rttConnectionStatus = BrainCloudRTT::RTTConnectionStatus::Disconnected;
        }
    }

    void RTTComms::closeSocket()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::closeSocket");
#endif
        std::unique_lock<std::mutex> lock(_socketMutex);

        if (_socket)
        {
            _socket->close();

            // We wait for the recv/send threads to shutdown
            _heartBeatMutex.lock();
            _heartbeatCondition.notify_one();
            _heartBeatMutex.unlock();
            if (_receivingRunning || _heartbeatRunning)
            {
                _threadsCondition.wait(lock, [this]()
                {
                    return !(_receivingRunning || _heartbeatRunning);
                });
            }

            delete _socket;
            _socket = NULL;
            if (_disconnectedWithReason == true)
            {
                Json::FastWriter myWriter;
                std::string output ="\n" + myWriter.write(_msg);
                printf("%s",output.c_str());
            }
        }
    }

    void RTTComms::enableRTT(IRTTConnectCallback* in_callback, bool in_useWebSocket)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::enableRTT");
#endif
        if(isRTTEnabled() || _rttConnectionStatus == BrainCloudRTT::RTTConnectionStatus::Connecting)
        {
            return;
        }
        else
        {
            _connectCallback = in_callback;
            _useWebSocket = in_useWebSocket;

            _appId = _context->getAppId();
            _sessionId = _context->getSessionId();

            s2s_log("request from %s", this);
            //s2s_log(static_cast<std::stringstream&&>(std::stringstream{} <<" JO JO JO JO request from "<<this<<std::endl<<std::flush));
            //auto pThis = std::unique_ptr<IServerCallback>((this));
            _context->getRTTService()->requestS2SConnection(this);
        }
    }

    void RTTComms::disableRTT()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::disableRTT");
#endif
        if(!isRTTEnabled() || _rttConnectionStatus == BrainCloudRTT::RTTConnectionStatus::Disconnecting)
        {
            return;
        }
        else
        {
            resetCommunication();
        }
    }

    bool RTTComms::isRTTEnabled()
    {
        return _rttConnectionStatus == BrainCloudRTT::RTTConnectionStatus::Connected;
    }

    BrainCloudRTT::RTTConnectionStatus RTTComms::getConnectionStatus()
    {
        return _rttConnectionStatus;
    }

    bool RTTComms::getLoggingEnabled()
    {
        return _loggingEnabled;
    }

    void RTTComms::enableLogging(bool isEnabled)
    {
        _loggingEnabled = isEnabled;
    }

    const std::string& RTTComms::getConnectionId()
    {
        return _connectionId;
    }

    void RTTComms::runCallbacks()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        // removing this from logging every method BECAUSE it prints out too often and wipes out other useful logs
        // keeping in code though since we may want to print this sometimes
        //s2s_log("VERBOSE: RTTComms::runCallbacks");
#endif
        _eventQueueMutex.lock();
        auto eventsCopy = _callbackEventQueue;
        _callbackEventQueue.clear();
        _eventQueueMutex.unlock();

        for (int i = 0; i < (int)eventsCopy.size(); ++i)
        {
            const RTTCallback& callback = eventsCopy[i];
            switch (callback._type)
            {
                case RTTCallbackType::ConnectSuccess:
                {
                    if (_connectCallback)
                    {
                        _connectCallback->rttConnectSuccess();
                    }
                    break;
                }
                case RTTCallbackType::ConnectFailure:
                {
                    if (_connectCallback)
                    {
                        _connectCallback->rttConnectFailure(callback._message);
                    }
                    break;
                }
                case RTTCallbackType::Event:
                {
                    std::string serviceName = callback._json["service"].asString();
                    std::map<std::string, IRTTCallback*>::iterator it = _callbacks.find(serviceName);
                    if (it != _callbacks.end())
                    {
                        it->second->rttCallback(callback._message);
                    }
                    break;
                }
            }
        }
    }

    void RTTComms::registerRTTCallback(const ServiceName& serviceName, IRTTCallback* in_callback)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::registerRTTCallback");
#endif
        _callbacks[serviceName.getValue()] = in_callback;
    }

    void RTTComms::deregisterRTTCallback(const ServiceName& serviceName)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::deregisterRTTCallback");
#endif
        std::map<std::string, IRTTCallback*>::iterator it = _callbacks.find(serviceName.getValue());
        if (it != _callbacks.end())
        {
            _callbacks.erase(it);
        }
    }

    void RTTComms::deregisterAllRTTCallbacks()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::deregisterAllRTTCallbacks");
#endif
        _callbacks.clear();
    }

    // IServerCallback
    void RTTComms::serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string& jsonData)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log(static_cast<std::stringstream&&>(std::stringstream{} << "VERBOSE: RTTComms::serverCallback()" << serviceName.getValue() << ", " << serviceOperation.getValue() << ", " << jsonData));
#endif
        s2s_log(static_cast<std::stringstream&&>(std::stringstream{} <<"serverCallback "<<std::endl));

        if (serviceName == ServiceName::RTTRegistration)
        {
            Json::Reader reader;
            Json::Value json;
            bool parsingSuccessful = reader.parse(jsonData, json);
            if (parsingSuccessful && json["status"].asInt() == 200)
                processRttRegistration(serviceOperation, json);
        }
    }

    void RTTComms::serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string& jsonError)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log(static_cast<std::stringstream&&>(std::stringstream{} << "VERBOSE: RTTComms::serverError()" << serviceName.getValue() << ", " << serviceOperation.getValue() << ", " << statusCode << ", " << reasonCode << ", " << jsonError));
#endif
        if (_connectCallback)
        {
            _connectCallback->rttConnectFailure(jsonError);
        }
    }

    void RTTComms::processRttRegistration(const ServiceOperation& serviceOperation, const Json::Value& jsonData)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::processRttRegistration");
#endif
        //REQUEST_SYSTEM_CONNECTION
        if (serviceOperation == ServiceOperation::RequestSystemConnection)
        {
            const Json::Value& data = jsonData["data"];
            _endpoint = getEndpointToUse(data["endpoints"]);
            if (_endpoint.isNull())
            {
                if (_connectCallback)
                {
                    _connectCallback->rttConnectFailure("No endpoint available");
                    return;
                }
            }

            _auth = data["auth"];
            //s2s_log("processing connect...");
            connect();
        }
    }

    Json::Value RTTComms::getEndpointToUse(const Json::Value& endpoints) const
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::getEndpointToUse");
#endif
        if (_useWebSocket)
        {
            //   1st choice: websocket + ssl
            //   2nd: websocket
            Json::Value endpoint = getEndpointForType(endpoints, "ws", true);
            if (!endpoint.isNull())
            {
                return endpoint;
            }
            return getEndpointForType(endpoints, "ws", false);
        }
        else
        {
            //   1st choice: tcp
            //   2nd: tcp + ssl (not implemented yet)
            Json::Value endpoint = getEndpointForType(endpoints, "tcp", false);
            if (!endpoint.isNull())
            {
                return endpoint;
            }
            return getEndpointForType(endpoints, "tcp", true);
        }
    }

    Json::Value RTTComms::getEndpointForType(const Json::Value& endpoints, const std::string& type, bool wantSsl)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        std::cout<<("VERBOSE: RTTComms::getEndpointForType") <<std::endl<<std::flush;
#endif
        for (int i = 0; i < (int)endpoints.size(); ++i)
        {
            const Json::Value& endpoint = endpoints[i];
            const std::string protocol = endpoint["protocol"].asString();
            if (protocol == type)
            {
                if (wantSsl)
                {
                    if (endpoint["ssl"].asBool())
                    {
                        return endpoint;
                    }
                }
                else
                {
                    return endpoint;
                }
            }
        }

        return Json::nullValue;
    }

    void RTTComms::connect()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::connect");
#endif
        _rttConnectionStatus = BrainCloudRTT::RTTConnectionStatus::Connecting;
#if (!defined(TARGET_OS_WATCH) || TARGET_OS_WATCH == 0)
        _disconnectedWithReason = false;

        std::thread connectionThread([this]
        {
            std::string host = _endpoint["host"].asString();
            int port = _endpoint["port"].asInt();
            std::vector<std::string> keys = _auth.getMemberNames();
            std::map<std::string, std::string> headers;
            for (int i = 0; i < (int)keys.size(); ++i)
            {
                headers[keys[i]] = _auth[keys[i]].asString();
            }

            {
                {
                    std::unique_lock<std::mutex> lock(_socketMutex);
                    if (_useWebSocket)
                    {
                        if (_endpoint["ssl"].asBool())
                        {
                            host = "wss://" + host;
                        }
                        else
                        {
                            host = "ws://" + host;
                        }

                        // Add headers to the query URL
                        if (!headers.empty())
                        {
                            host += "?";
                            for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it)
                            {
                                const std::string& key = it->first;
                                const std::string& value = it->second;

                                if (host.back() != '?')
                                {
                                    host += "&";
                                }
                                host += key + "=" + value;
                            }
                        }
                        #ifndef LIBWEBSOCKETS_OFF
                        // only creates this object if the required files are linked in
                        // could arise if libwebsockets are OFF in the makefile but ON in the app
                        // in this case, there WILL be a connection error called after enableRTT()
                        _socket = IWebSocket::create(host, port, headers);
                        #endif
                    }
                    else
                    {
                        #ifdef USE_TCP
                        _socket = ITCPSocket::create(host, port);
                        #endif
                    }
                }
                if (!_socket || !_socket->isValid())
                {
                    closeSocket();
                    failedToConnect();
                    _rttConnectionStatus = BrainCloudRTT::RTTConnectionStatus::Disconnected;
                    return;
                }

                _rttConnectionStatus = BrainCloudRTT::RTTConnectionStatus::Connected;
            }

            _lastHeartbeatTime = TimeUtil::getCurrentTimeMillis();

            if (_loggingEnabled)
            {
                std::cout << "RTT: connected" << std::endl;
            }

            onSocketConnected();
        });
        connectionThread.detach();
#else
        failedToConnect();
#endif
    }

    void RTTComms::failedToConnect()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::failedToConnect");
#endif
        std::string host;
        int port = 0;

        if (!_endpoint.isNull())
        {
            host = _endpoint["host"].asString();
            port = _endpoint["port"].asInt();
        }

        _eventQueueMutex.lock();
        _callbackEventQueue.push_back(RTTCallback(RTTCallbackType::ConnectFailure, "Failed to connect to RTT Event server: " + host + ":" + std::to_string(port)));
        _eventQueueMutex.unlock();
    }

    Json::Value RTTComms::buildConnectionRequest(const std::string& protocol)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::buildConnectionRequest");
#endif
        Json::Value json;
        json["operation"] = "CONNECT";
        json["service"] = "rtt";

        Json::Value system;
        system["protocol"] = protocol;
        system["platform"] = "C++";

        Json::Value jsonData;
        jsonData["appId"] = _appId;
        jsonData["profileId"] = "s";
        jsonData["sessionId"] = _sessionId;
        jsonData["auth"] = _auth;
        jsonData["system"] = system;
        json["data"] = jsonData;

        return json;
    }

    void RTTComms::onSocketConnected()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::onSocketConnected");
#endif
        startReceiving();
        s2s_log(static_cast<std::stringstream&&>(std::stringstream{} <<"why am I doing this again"<<std::endl));
        if (!send(buildConnectionRequest(_useWebSocket ? "ws" : "tcp")))
        {
            failedToConnect();
        }
    }

    bool RTTComms::send(const Json::Value& jsonData)
    {
        if (_loggingEnabled)
        {
            Json::StyledWriter writer;
            std::string message = writer.write(jsonData);
            std::cout << "RTT SEND " << message << std::endl;
        }

        std::unique_lock<std::mutex> lock(_socketMutex);
        if (isRTTEnabled())
        {
            Json::FastWriter writer;
            std::string message = writer.write(jsonData);

            s2s_log(static_cast<std::stringstream&&>(std::stringstream{} <<"sending "<<message<<std::endl));
            _socket->send(message);
        }

        return true;
    }

    void RTTComms::startReceiving()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::startReceiving");
#endif
        _receivingRunning = true;
        std::thread receiveThread([this]
        {
            while (isRTTEnabled())
            {
                std::string message = _socket->recv();
                if (message.empty())
                {
                    break;
                }
                onRecv(message);
            }

            std::unique_lock<std::mutex> lock(_socketMutex);
            _receivingRunning = false;
            _threadsCondition.notify_one();
        });
        receiveThread.detach();
    }

    void RTTComms::startHeartbeat()
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log("VERBOSE: RTTComms::startHeartbeat");
#endif
        _heartbeatRunning = true;
        std::thread heartbeatThread([this]
        {
            Json::Value jsonHeartbeat;
            jsonHeartbeat["operation"] = "HEARTBEAT";
            jsonHeartbeat["service"] = "rtt";

            std::unique_lock<std::mutex> lock(_heartBeatMutex);

            while (isRTTEnabled())
            {
                int64_t sleepTime = ((int64_t)_heartbeatSeconds * 1000) - (TimeUtil::getCurrentTimeMillis() - _lastHeartbeatTime);
                if (sleepTime > 0)
                {
                    _heartbeatCondition.wait_for(lock, std::chrono::milliseconds(sleepTime));
                }
                else
                {
                    send(jsonHeartbeat);
                    _lastHeartbeatTime = TimeUtil::getCurrentTimeMillis();
                }
            }

            _heartbeatRunning = false;
            _threadsCondition.notify_one();
        });
        heartbeatThread.detach();
    }

    void RTTComms::onRecv(const std::string& message)
    {
        if (_loggingEnabled)
        {
            std::cout << "RTT RECV: " << message << std::endl;
        }

        Json::Reader reader;
        Json::Value jsonData;
        if (!reader.parse(message, jsonData))
        {
            failedToConnect();
            return;
        }

        std::string serviceName = jsonData["service"].asString();
        printf("  serviceName: %s, %s\n", serviceName.c_str(), message.c_str());
        processRttMessage(jsonData, message);
    }

    void RTTComms::processRttMessage(const Json::Value& json, const std::string& message)
    {
#if RTTCOMMS_LOG_EVERY_METHODS
        s2s_log(static_cast<std::stringstream&&>(std::stringstream{} << "VERBOSE: RTTComms::processRttMessage(" << message));
#endif

        std::string serviceName = json["service"].asString();
        std::string operation = json["operation"].asString();
        s2s_log(static_cast<std::stringstream&&>(std::stringstream{} <<serviceName.c_str()<<message.c_str()<<std::endl));
        if (serviceName == "rtt")
        {
            if (operation == "CONNECT")
            {
                _heartbeatSeconds = json["data"].get("heartbeatSeconds", 30).asInt();
                _connectionId = json["data"]["cxId"].asString();

                startHeartbeat();

                _eventQueueMutex.lock();
                _callbackEventQueue.push_back(RTTCallback(RTTCallbackType::ConnectSuccess));
                _eventQueueMutex.unlock();
            }
            else if (operation == "DISCONNECT")
            {
                _disconnectedWithReason = true;
                _disconnectReasonMessage = json["data"]["reason"].asString();
                _disconnectReasonCode = json["data"]["reasonCode"].asInt();

			    _msg["reasonCode"] = _disconnectReasonCode;
			    _msg["reason"] = _disconnectReasonMessage;
            }
        }
        else
        {
            _eventQueueMutex.lock();
            _callbackEventQueue.push_back(RTTCallback(RTTCallbackType::Event, json, message));
            _eventQueueMutex.unlock();
        }
    }
}