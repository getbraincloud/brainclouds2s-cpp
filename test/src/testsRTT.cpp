#include "catch.hpp"
#include "tests.h"


///////////////////////////////////////////////////////////////////////////////
// RTT Flow scenario
///////////////////////////////////////////////////////////////////////////////
TEST_CASE("RTT EnableRTTSync", "[S2S]")
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

    SECTION("RTT EnableRTTSync")
    {
        // 1. authenticate (if autoAuth is false)
        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);

        // 2. enable rtt and hookup connect callback
        std::string ret = pContext->enableRTTSync();

        // 3. ensure we got a good response (no error message)
        Json::Value data;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(ret.c_str(), data);
        REQUIRE(ret.empty());

        // 4. ensure rtt has been enabled
        REQUIRE(pContext->getRTTService()->getRTTEnabled());
    }
}

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

    SECTION("RTT EnableRTT")
    {
        // 1. authenticate (if autoAuth is false)
        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);

        // 2. enable rtt and hookup connect callback
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

        while (!rttConnectCallback.processed)
        {
            pContext->runCallbacks();
        }

        // 3. ensure we got a good response (no error message)
        REQUIRE(rttConnectCallback.processed);
        REQUIRE(rttConnectCallback.ret.empty());

        // 4. ensure rtt has been enabled
        REQUIRE(rttService->getRTTEnabled());
    }
}

TEST_CASE("RTT EnableRTT - Auto auth", "[S2S]")
{
    loadIdsIfNot();
    auto pContext = S2SContext::create(
            BRAINCLOUD_APP_ID,
            BRAINCLOUD_SERVER_NAME,
            BRAINCLOUD_SERVER_SECRET,
            BRAINCLOUD_SERVER_URL,
            true
    );
    pContext->setLogEnabled(true);


    SECTION("RTT EnableRTT")
    {
        // 1. don't authenticate (should be done above)

        // 2. enable rtt and hookup connect callback
        std::string ret = pContext->enableRTTSync();

        // 3. ensure we got a good response (no error message)
        Json::Value data;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(ret.c_str(), data);
        REQUIRE(ret.empty());

        // 4. ensure rtt has been enabled
        REQUIRE(pContext->getRTTService()->getRTTEnabled());
    }
}