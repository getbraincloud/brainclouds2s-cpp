#include "catch.hpp"
#include "tests.h"


///////////////////////////////////////////////////////////////////////////////
// RTT Flow scenario
///////////////////////////////////////////////////////////////////////////////
TEST_CASE("RTT Flow", "[S2S]")
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

    SECTION("RTT FLOW")
    {
        // 1. authenticate (if autoAuth is false)
        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);

        // 2. enable rtt and hookup connect callback
        BrainCloudRTT* rttService = pContext->getRTTService();

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

        rttService->enableRTT(&rttConnectCallback, true);

        while (!rttConnectCallback.processed)
        {
            pContext->runCallbacks();
        }

 //        pContext->enableRTTSync();

        REQUIRE(rttService->getRTTEnabled());
    }
}
