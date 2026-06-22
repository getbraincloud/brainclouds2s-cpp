// Copyright 2026 bitHeads, Inc. All Rights Reserved.

#include "brainclouds2s-prl.h"
#include "brainclouds2s-rtt.h"
#include "ServiceName.h"
#include "json/json.h"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace BrainCloud
{
    // --------------------------------------------------------------------------
    // Environment helpers
    // --------------------------------------------------------------------------

    bool BrainCloudS2SPRL::isPreReadyLaunch() const
    {
        const char* val = std::getenv("PRE_READY_LAUNCH");
        if (!val) return false;
        std::string s(val);
        for (char& c : s) c = (char)tolower((unsigned char)c);
        return s == "true";
    }

    int BrainCloudS2SPRL::getTimeoutSecs() const
    {
        const char* val = std::getenv("PRL_TIMEOUT_SECS");
        if (val)
        {
            try { return std::stoi(val); } catch (...) {}
        }
        val = std::getenv("PRE_READY_LAUNCH_TIMEOUT_SECS");
        if (val)
        {
            try { return std::stoi(val); } catch (...) {}
        }
        return 60;
    }

    std::string BrainCloudS2SPRL::parseServerContext() const
    {
        const char* raw = std::getenv("SERVER_CONTEXT");
        if (!raw) return "{}";
        std::string val(raw);
        // Strip surrounding single quotes if present
        if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'')
            val = val.substr(1, val.size() - 2);
        // Unescape backslash-escaped double quotes
        std::string out;
        out.reserve(val.size());
        for (size_t i = 0; i < val.size(); ++i)
        {
            if (val[i] == '\\' && i + 1 < val.size() && val[i + 1] == '"')
            {
                out += '"';
                ++i;
            }
            else
            {
                out += val[i];
            }
        }
        // Validate: if it doesn't look like JSON, return {}
        if (out.empty() || out.front() != '{') return "{}";
        return out;
    }

    bool BrainCloudS2SPRL::isComplete() const
    {
        return _complete.load();
    }

    // --------------------------------------------------------------------------
    // sendSessionEnded
    // --------------------------------------------------------------------------

    void BrainCloudS2SPRL::sendSessionEnded(S2SContextRef s2s, const S2SCallback& callback)
    {
        log("[PRL] Sending SYS_ROOM_SESSION_ENDED.");
        Json::Value data;
        data["serverId"] = std::string(std::getenv("SERVER_ID") ? std::getenv("SERVER_ID") : "");

        Json::Value serverCtx;
        Json::Reader reader;
        reader.parse(parseServerContext(), serverCtx);
        data["serverContext"] = serverCtx;

        Json::Value request;
        request["service"] = "roomServer";
        request["operation"] = "SYS_ROOM_SESSION_ENDED";
        request["data"] = data;

        Json::FastWriter writer;
        std::string reqStr = writer.write(request);
        if (!reqStr.empty() && reqStr.back() == '\n') reqStr.pop_back();
        s2s->request(reqStr, callback);
    }

    // --------------------------------------------------------------------------
    // start — main PRL flow
    // --------------------------------------------------------------------------

    void BrainCloudS2SPRL::start(
        S2SContextRef s2s, const std::string& lobbyId, PRLCompleteCallback callback)
    {
        _s2s = s2s;
        _lobbyId = lobbyId;
        _callback = callback;
        _state = PrlState::ConnectingRTT;
        _complete.store(false);

        int timeoutSecs = getTimeoutSecs();
        log("[PRL] Starting PRL flow. lobbyId=" + lobbyId +
            ", timeoutSecs=" + std::to_string(timeoutSecs));

        // Register for Chat RTT events to receive lobby state pushes
        s2s->getRTTService()->registerRTTCallback(ServiceName::Chat, this);

        // Start timeout timer on a background thread
        if (timeoutSecs > 0)
        {
            std::thread([this, timeoutSecs]()
            {
                std::this_thread::sleep_for(std::chrono::seconds(timeoutSecs));
                if (!_complete.load())
                {
                    log("[PRL] Timeout elapsed — exiting.");
                    complete(false);
                }
            }).detach();
        }

        // Step 1: Enable RTT (this as IRTTConnectCallback)
        s2s->enableRTT(this);
    }

    // --------------------------------------------------------------------------
    // IRTTConnectCallback
    // --------------------------------------------------------------------------

    void BrainCloudS2SPRL::rttConnectSuccess()
    {
        if (_complete.load()) return;

        std::string channelId = buildChannelId();
        log("[PRL] RTT connected. Subscribing to channel: " + channelId);
        _state = PrlState::SubscribingChannel;

        // Step 2: Subscribe to the lobby status channel
        Json::Value channelData;
        channelData["channelId"] = channelId;
        channelData["maxReturn"] = 50;
        Json::Value channelRequest;
        channelRequest["service"] = "chat";
        channelRequest["operation"] = "CHANNEL_CONNECT";
        channelRequest["data"] = channelData;

        Json::FastWriter writer;
        std::string channelReqStr = writer.write(channelRequest);
        if (!channelReqStr.empty() && channelReqStr.back() == '\n') channelReqStr.pop_back();

        _s2s->request(channelReqStr, [this](const std::string& channelResult)
        {
            if (_complete.load()) return;

            Json::Value resp;
            Json::Reader reader;
            if (!reader.parse(channelResult, resp) || resp["status"].asInt() != 200)
            {
                log("[PRL] Failed to subscribe to channel: " + channelResult);
                complete(false);
                return;
            }

            log("[PRL] Channel subscribed. Notifying session started.");
            _state = PrlState::NotifyingSessionStarted;

            // Step 3: Notify brainCloud the room session has started
            Json::Value sessionData;
            sessionData["serverId"] = std::string(
                std::getenv("SERVER_ID") ? std::getenv("SERVER_ID") : "");
            Json::Value serverCtx;
            Json::Reader ctxReader;
            ctxReader.parse(parseServerContext(), serverCtx);
            sessionData["serverContext"] = serverCtx;

            Json::Value sessionRequest;
            sessionRequest["service"] = "roomServer";
            sessionRequest["operation"] = "SYS_ROOM_SESSION_STARTED";
            sessionRequest["data"] = sessionData;

            Json::FastWriter fw;
            std::string sessionReqStr = fw.write(sessionRequest);
            if (!sessionReqStr.empty() && sessionReqStr.back() == '\n') sessionReqStr.pop_back();

            _s2s->request(sessionReqStr, [this](const std::string& sessionResult)
            {
                if (_complete.load()) return;

                Json::Value resp;
                Json::Reader reader;
                if (!reader.parse(sessionResult, resp) || resp["status"].asInt() != 200)
                {
                    log("[PRL] SYS_ROOM_SESSION_STARTED failed: " + sessionResult);
                    complete(false);
                    return;
                }

                log("[PRL] Session started. Querying lobby state.");
                _state = PrlState::QueryingLobbyState;

                // Step 4: Query the current lobby state
                Json::Value lobbyData;
                lobbyData["lobbyId"] = _lobbyId;
                Json::Value lobbyRequest;
                lobbyRequest["service"] = "lobby";
                lobbyRequest["operation"] = "GET_LOBBY_DATA";
                lobbyRequest["data"] = lobbyData;

                Json::FastWriter fw2;
                std::string lobbyReqStr = fw2.write(lobbyRequest);
                if (!lobbyReqStr.empty() && lobbyReqStr.back() == '\n') lobbyReqStr.pop_back();

                _s2s->request(lobbyReqStr, [this](const std::string& lobbyResult)
                {
                    std::string lobbyState = parseLobbyState(lobbyResult);
                    log("[PRL] Initial lobby state: " +
                        (lobbyState.empty() ? "null" : lobbyState));
                    handleLobbyState(lobbyState);
                });
            });
        });
    }

    void BrainCloudS2SPRL::rttConnectFailure(const std::string& errorMessage)
    {
        log("[PRL] RTT connection failed: " + errorMessage);
        complete(false);
    }

    // --------------------------------------------------------------------------
    // IRTTCallback — lobby state pushes arrive here
    // --------------------------------------------------------------------------

    void BrainCloudS2SPRL::rttCallback(const std::string& jsonData)
    {
        if (_complete.load() || _state != PrlState::WaitingForLobbyReady) return;
        std::string lobbyState = parseLobbyStateFromRTT(jsonData);
        if (!lobbyState.empty())
        {
            log("[PRL] RTT lobby state update: " + lobbyState);
            handleLobbyState(lobbyState);
        }
    }

    // --------------------------------------------------------------------------
    // Internal helpers
    // --------------------------------------------------------------------------

    void BrainCloudS2SPRL::complete(bool proceed)
    {
        if (_complete.exchange(true)) return; // already completed
        _state = PrlState::Complete;

        if (_s2s)
            _s2s->getRTTService()->deregisterRTTCallback(ServiceName::Chat);

        if (_callback)
            _callback(proceed);
    }

    std::string BrainCloudS2SPRL::buildChannelId() const
    {
        // Lobby ID format: <appId>:<instanceId>
        // Channel format:  <appId>:sy:_lobby_<instanceId>
        std::string instanceId = _lobbyId;
        size_t colonPos = _lobbyId.find(':');
        if (colonPos != std::string::npos)
            instanceId = _lobbyId.substr(colonPos + 1);
        return _s2s->getAppId() + ":sy:_lobby_" + instanceId;
    }

    std::string BrainCloudS2SPRL::parseLobbyState(const std::string& resultJson) const
    {
        // GET_LOBBY_DATA response: { "status": 200, "data": { "state": "..." } }
        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(resultJson, data)) return "";
        if (data["status"].asInt() != 200) return "";
        return data["data"]["state"].asString();
    }

    std::string BrainCloudS2SPRL::parseLobbyStateFromRTT(const std::string& jsonData) const
    {
        // RTT push: { "service":"chat","operation":"INCOMING",
        //             "data":{ "content":{ "data":{ "lobby":{ "state":"..." } } } } }
        Json::Value msg;
        Json::Reader reader;
        if (!reader.parse(jsonData, msg)) return "";
        if (msg["service"].asString() != "chat") return "";
        if (msg["operation"].asString() != "INCOMING") return "";
        try
        {
            return msg["data"]["content"]["data"]["lobby"]["state"].asString();
        }
        catch (...) {}
        return "";
    }

    void BrainCloudS2SPRL::handleLobbyState(const std::string& lobbyState)
    {
        if (lobbyState.empty())
        {
            log("[PRL] Lobby not found — exiting.");
            complete(false);
        }
        else if (lobbyState == "disbanded")
        {
            log("[PRL] Lobby disbanded — exiting.");
            complete(false);
        }
        else if (lobbyState == "starting")
        {
            log("[PRL] Lobby is starting — proceeding with launch.");
            complete(true);
        }
        else
        {
            log("[PRL] Lobby state is '" + lobbyState + "' — waiting for RTT update.");
            _state = PrlState::WaitingForLobbyReady;
        }
    }

    void BrainCloudS2SPRL::log(const std::string& message) const
    {
        s2s_log("#BCC ", message);
    }

} // namespace BrainCloud
