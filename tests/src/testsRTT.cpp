#include "tests.h"
#include "catch.hpp"
#include <cstdio>

// Writes directly to stderr so step logs appear live regardless of Catch's stdout capture
#define RTT_LOG(msg) do { fprintf(stderr, "%s\n", msg); fflush(stderr); } while(0)

///////////////////////////////////////////////////////////////////////////////
// RTT Flow scenario
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("RTT EnableRTT", "[S2S]")
{
    loadIdsIfNot();
    auto pContext = S2SContext::create(
            BRAINCLOUD_APP_ID,
            BRAINCLOUD_SERVER_NAME,
            BRAINCLOUD_SERVER_SECRET,
            BRAINCLOUD_SERVER_URL,
            false
    );
    pContext->setLogEnabled(true);

    SECTION("RTT Connection")
    {
        RTT_LOG("Step 1: Authenticating...");
        auto retAuth = runAuth(pContext);
        RTT_LOG(retAuth ? "Step 1: Auth succeeded" : "Step 1: Auth FAILED");
        REQUIRE(retAuth);

        // brainCloud RTT Connection callbacks
        class TestConnectCallback final : public BrainCloud::IRTTConnectCallback
        {
        public:
            std::string ret;
            bool processed = false;
            void rttConnectSuccess() override{
                processed = true;
            };

            void rttConnectFailure(const std::string& errorMessage) override{
                processed = true;
                ret = errorMessage;
            };
        }rttConnectCallback;

        RTT_LOG("Step 2: Enabling RTT...");
        BrainCloudRTT* rttService = pContext->getRTTService();
        rttService->enableRTT(&rttConnectCallback, true);

        bool done = false;
        do
        {
            pContext->runCallbacks();

            if(rttConnectCallback.processed) {
                done = true;
            }
        }while (!done);

        if (rttConnectCallback.ret.empty()) {
            RTT_LOG("Step 2: RTT connected successfully");
        } else {
            fprintf(stderr, "Step 2: RTT connect FAILED: %s\n", rttConnectCallback.ret.c_str());
            fflush(stderr);
        }

        // 3. ensure we got a good response (no error message)
        REQUIRE(rttConnectCallback.processed);
        REQUIRE(rttConnectCallback.ret.empty());

        // 4. ensure rtt has been enabled
        REQUIRE(rttService->getRTTEnabled());
        RTT_LOG("Step 3: RTT enabled confirmed");
    }
}

TEST_CASE("RTT RegisterCallbacks", "[S2S]") {

    loadIdsIfNot();
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
    );

    SECTION("RTT Connection")
    {
        pContext->setLogEnabled(true);

        RTT_LOG("Step 1: Authenticating...");
        auto retAuth = runAuth(pContext);
        RTT_LOG(retAuth ? "Step 1: Auth succeeded" : "Step 1: Auth FAILED");
        REQUIRE(retAuth);

        BrainCloudRTT* rttService = pContext->getRTTService();

        class TestRTTCallback final : public BrainCloud::IRTTCallback {
        public:
            bool receivedCallback = false;
            void rttCallback(const std::string& jsonData) {
                receivedCallback = true;
                s2s_log("Received RTT Callback: ", jsonData);
                fprintf(stderr, "Step 6: RTT callback received\n");
                fflush(stderr);
            }
        }testRTTCallback;
        rttService->registerRTTCallback(ServiceName::Chat, &testRTTCallback);

        // brainCloud RTT Connection callbacks
        class TestConnectCallback final : public BrainCloud::IRTTConnectCallback
        {
        public:
            std::string ret;
            bool processed = false;
            void rttConnectSuccess() override {
                processed = true;
            };

            void rttConnectFailure(const std::string& errorMessage) override {
                processed = true;
                ret = errorMessage;
            };
        }rttConnectCallback;

        RTT_LOG("Step 2: Enabling RTT...");
        rttService->enableRTT(&rttConnectCallback, true);

        bool done = false;
        do
        {
            pContext->runCallbacks();

            if (rttConnectCallback.processed) {
                done = true;
            }
        } while (!done);

        if (rttConnectCallback.ret.empty()) {
            RTT_LOG("Step 2: RTT connected successfully");
        } else {
            fprintf(stderr, "Step 2: RTT connect FAILED: %s\n", rttConnectCallback.ret.c_str());
            fflush(stderr);
        }

        REQUIRE(rttConnectCallback.processed);
        REQUIRE(rttConnectCallback.ret.empty());
        REQUIRE(rttService->getRTTEnabled());

        auto joinChannelRequest = "{ \
            \"service\": \"chat\", \
            \"operation\": \"SYS_CHANNEL_CONNECT\", \
            \"data\": { \
                \"ccCall\": false, \
                \"channelId\": \"20001:sy:test\", \
                \"maxReturn\": 2 \
            } \
        }";

        RTT_LOG("Step 3: Joining chat channel...");
        auto ret = run(joinChannelRequest, pContext);
        RTT_LOG(ret ? "Step 3: Channel joined" : "Step 3: Channel join FAILED");
        REQUIRE(ret);

        auto sendMessageRequest = "{ \
            \"service\": \"chat\", \
            \"operation\": \"SYS_POST_CHAT_MESSAGE\", \
            \"data\": { \
                \"ccCall\": false, \
                \"channelId\": \"20001:sy:test\", \
                \"maxReturn\": 0, \
                \"content\": { \
                    \"text\": \"Hello world from s2s test\"\
                }, \
                \"recordInHistory\": true, \
                \"from\" : { \
                    \"name\": \"Homer\", \
                    \"pic\" : \"http://www.simpsons.test/homer.jpg\"  \
                }  \
            } \
        }";

        RTT_LOG("Step 4: Sending chat message...");
        ret = run(sendMessageRequest, pContext);
        RTT_LOG(ret ? "Step 4: Message sent" : "Step 4: Message send FAILED");
        REQUIRE(ret);

        RTT_LOG("Step 5: Waiting for RTT callback (up to 10s)...");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!testRTTCallback.receivedCallback &&
               std::chrono::steady_clock::now() < deadline)
        {
            pContext->runCallbacks(100);
        }

        if (!testRTTCallback.receivedCallback) {
            RTT_LOG("Step 5: Timed out waiting for RTT callback");
        }

        REQUIRE(testRTTCallback.receivedCallback);
    }
}
