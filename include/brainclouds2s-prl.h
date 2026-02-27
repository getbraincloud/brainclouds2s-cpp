// Copyright 2026 bitHeads, Inc. All Rights Reserved.

#pragma once

#include "brainclouds2s.h"
#include "IRTTConnectCallback.h"
#include "IRTTCallback.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace BrainCloud
{
    /**
     * Handles the Pre-Ready Launch (PRL) flow for custom servers launched by brainCloud.
     *
     * When the PRE_READY_LAUNCH environment variable is "true", the server must wait
     * for the assigned lobby to reach the "starting" state before proceeding with launch.
     *
     * Usage:
     *   BrainCloudS2SPRL prl;
     *   if (prl.isPreReadyLaunch()) {
     *       prl.start(ctx, lobbyId, [](bool proceed) {
     *           if (proceed) { ... } else { exit(0); }
     *       });
     *       while (!prl.isComplete()) {
     *           ctx->runCallbacks();
     *           // sleep briefly
     *       }
     *   }
     *
     * Note: The BrainCloudS2SPRL object must remain alive until isComplete() returns true.
     */
    class BrainCloudS2SPRL : public IRTTConnectCallback, public IRTTCallback
    {
    public:
        using PRLCompleteCallback = std::function<void(bool proceedWithLaunch)>;

        BrainCloudS2SPRL() = default;
        ~BrainCloudS2SPRL() = default;

        /** Returns true if the PRE_READY_LAUNCH env var is set to "true". */
        bool isPreReadyLaunch() const;

        /**
         * Returns the timeout in seconds from PRL_TIMEOUT_SECS or
         * PRE_READY_LAUNCH_TIMEOUT_SECS environment variables. Default: 60.
         */
        int getTimeoutSecs() const;

        /**
         * Parses the SERVER_CONTEXT environment variable into a JSON string.
         * Handles single-quoted and backslash-escaped values.
         */
        std::string parseServerContext() const;

        /** Returns true when the PRL flow has completed. */
        bool isComplete() const;

        /**
         * Starts the PRL flow. The S2S context must already be authenticated.
         *
         * Flow:
         *   1. Enable RTT
         *   2. Subscribe to the lobby status channel (chat/CHANNEL_CONNECT)
         *   3. Notify brainCloud the room session has started (roomServer/SYS_ROOM_SESSION_STARTED)
         *   4. Query lobby state (lobby/GET_LOBBY_DATA)
         *   5. If "starting" → proceed. If "disbanded" or not found → exit.
         *      Otherwise wait for RTT push.
         *
         * @param s2s      Authenticated S2S context
         * @param lobbyId  The lobby ID assigned to this server
         * @param callback Called with true to proceed with launch, false to exit
         */
        void start(S2SContextRef s2s, const std::string& lobbyId, PRLCompleteCallback callback);

        /**
         * Sends SYS_ROOM_SESSION_ENDED to notify brainCloud the session has concluded.
         * Should be called before the server process exits.
         *
         * @param s2s      Authenticated S2S context
         * @param callback Optional callback; may be nullptr
         */
        void sendSessionEnded(S2SContextRef s2s, const S2SCallback& callback);

        // IRTTConnectCallback
        void rttConnectSuccess() override;
        void rttConnectFailure(const std::string& errorMessage) override;

        // IRTTCallback
        void rttCallback(const std::string& jsonData) override;

    private:
        enum class PrlState
        {
            Idle,
            ConnectingRTT,
            SubscribingChannel,
            NotifyingSessionStarted,
            QueryingLobbyState,
            WaitingForLobbyReady,
            Complete
        };

        S2SContextRef _s2s;
        std::string _lobbyId;
        PRLCompleteCallback _callback;
        PrlState _state = PrlState::Idle;
        std::atomic<bool> _complete{false};

        void complete(bool proceed);
        std::string buildChannelId() const;
        std::string parseLobbyState(const std::string& resultJson) const;
        std::string parseLobbyStateFromRTT(const std::string& jsonData) const;
        void handleLobbyState(const std::string& lobbyState);
        void log(const std::string& message) const;
    };

} // namespace BrainCloud
