#include "catch.hpp"
#include "tests.h"


///////////////////////////////////////////////////////////////////////////////
// Test logs
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("Logs LogToFile", "[S2S]")
{
    loadIdsIfNot();
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
    );

    std::string logPath = getExecutableDir() + "/testLog.log";

    logToFile(logPath);

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
            void rttConnectSuccess() override {
                processed = true;
            };

            void rttConnectFailure(const std::string& errorMessage) override {
                processed = true;
                ret = errorMessage;
            };
        }rttConnectCallback;

        BrainCloudRTT* rttService = pContext->getRTTService();
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
    }
}