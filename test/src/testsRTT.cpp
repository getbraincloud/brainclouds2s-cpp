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
            true
    );
    pContext->setLogEnabled(true);

    printf("testing rtt");

    SECTION("RTT FLOW")
    {
        // 1. authenticate
        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);

        // 2. enable rtt and hookup connect callback
        BrainCloudRTT* rttService = pContext->getRTTService();
        RTTS2SConnectCallback rttConnectCallback;
        rttService->enableRTT(&rttConnectCallback, true);

        while (!rttConnectCallback.processed)
        {
            pContext->runCallbacks(100);
        }

        REQUIRE(rttService->getRTTEnabled());
    }
}
