#include "tests.h"
#include "catch.hpp"

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
        // 1. authenticate (if autoAuth is false)
        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);

        // 2. enable rtt and hookup connect callback
        bool done = false;

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

        BrainCloudRTT* rttService = pContext->getRTTService();
        rttService->enableRTT(&rttConnectCallback, true);

        do
        {
            pContext->runCallbacks();

            if(rttConnectCallback.processed) {
                done = true;
            }
        }while (!done);

        // 3. ensure we got a good response (no error message)
        REQUIRE(rttConnectCallback.processed);
        REQUIRE(rttConnectCallback.ret.empty());

        // 4. ensure rtt has been enabled
        REQUIRE(rttService->getRTTEnabled());
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
    pContext->setLogEnabled(true);

    SECTION("RTT Connection")
    {
        // 1. authenticate (if autoAuth is false)
        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);
        BrainCloudRTT* rttService = pContext->getRTTService();
        // 2. enable rtt and hookup connect callback
        bool done = false;

        class TestRTTCallback final : public BrainCloud::IRTTCallback {
        public:
            bool receivedCallback = false;
            void rttCallback(const std::string& jsonData) {
                INFO("RECEIVED RTT CALLBACK : " + jsonData);
                receivedCallback = true;
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

        
        rttService->enableRTT(&rttConnectCallback, true);

        do
        {
            pContext->runCallbacks();

            if (rttConnectCallback.processed) {
                done = true;
            }
        } while (!done);

        // 3. ensure we got a good response (no error message)
        REQUIRE(rttConnectCallback.processed);
        REQUIRE(rttConnectCallback.ret.empty());

        // 4. ensure rtt has been enabled
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

        auto ret = run(joinChannelRequest, pContext);
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

        ret = run(sendMessageRequest, pContext);
        REQUIRE(ret);

        std::this_thread::sleep_for(std::chrono::seconds(2));

       REQUIRE(testRTTCallback.receivedCallback);
    }
}