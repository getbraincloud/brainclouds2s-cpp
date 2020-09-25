#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "ids.h"

#include <brainclouds2s.h>
#include <json/json.h>

bool run(const std::string& request, S2SContextRef pContext)
{
    bool processed = false;
    std::string ret;

    pContext->request(request, [&](const std::string& result)
    {
        ret = result;
        processed = true;
    });
    
    while (!processed)
    {
        pContext->runCallbacks(100);
    }

    Json::Value data;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(ret.c_str(), data);
    if (!parsingSuccessful)
    {
        return false;
    }

    return data["status"].asInt() == 200;
}

void wait(S2SContextRef pContext)
{
    int times = 5;
    while (times-- > 0)
    {
        pContext->runCallbacks(5000);
    }
}

TEST_CASE("Create context", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL
    );
    REQUIRE(pContext);

    auto pAnotherContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL
    );
    REQUIRE(pAnotherContext);
    REQUIRE_FALSE(pContext == pAnotherContext);

    pContext = nullptr;
    pAnotherContext = nullptr;
}

TEST_CASE("Valid Context", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL
    );
    pContext->setLogEnabled(true);

    SECTION("Run Script")
    {
        auto request = "{ \
            \"service\": \"script\", \
            \"operation\": \"RUN\", \
            \"data\": { \
                \"scriptName\": \"testScript2\" \
            } \
        }";

        auto ret = run(request, pContext);
        REQUIRE(ret);
    }

    SECTION("Successive calls")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        int processed_count = 0;
        int success_count = 0;

        auto callback = [&](const std::string& result)
        {
            Json::Value data;
            Json::Reader reader;
            bool parsingSuccessful = reader.parse(result.c_str(), data);
            if (parsingSuccessful && data["status"].asInt() == 200)
            {
                success_count++;
            }
            processed_count++;
        };

        // Queue many at once
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        
        while (processed_count < 5)
        {
            pContext->runCallbacks(100);
        }

        REQUIRE(success_count == 5);
    }

    SECTION("Bad Request Json")
    {
        auto request = "Bad Request";
        auto ret = run(request, pContext);
        REQUIRE_FALSE(ret);
    }

    SECTION("Bad Request")
    {
        auto request = "{ \
            \"service\": \"bad_service\", \
            \"operation\": \"bad_op\", \
            \"data\": {} \
        }";
        auto ret = run(request, pContext);
        REQUIRE_FALSE(ret);
    }
}

TEST_CASE("Context with bad Server Secret", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        "null",
        BRAINCLOUD_SERVER_URL
    );
    REQUIRE(pContext);

    pContext->setLogEnabled(true);

    auto request = "{ \
        \"service\": \"time\", \
        \"operation\": \"READ\", \
        \"data\": {} \
    }";

    auto ret = run(request, pContext); // Should fail on auth
    REQUIRE_FALSE(ret);
}

TEST_CASE("RunCallbacks with timeout", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL
    );

    pContext->setLogEnabled(true);

    auto request = "{ \
        \"service\": \"time\", \
        \"operation\": \"READ\", \
        \"data\": {} \
    }";

    Json::Value data;
    bool processed = false;
    std::string ret;

    pContext->request(request, [&](const std::string& result)
    {
        ret = result;
        processed = true;
    });

    pContext->runCallbacks(20 * 1000); // Auth
    pContext->runCallbacks(20 * 1000); // The request

    REQUIRE(processed);

    Json::Reader reader;
    bool parsingSuccessful = reader.parse(ret.c_str(), data);
    REQUIRE(parsingSuccessful);

    auto status_code = data["status"].asInt();
    REQUIRE(status_code == 200);
}

TEST_CASE("requestSync", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL
    );

    pContext->setLogEnabled(true);

    auto request = "{ \
        \"service\": \"time\", \
        \"operation\": \"READ\", \
        \"data\": {} \
    }";

    Json::Value data;
    bool processed = false;

    std::string result = pContext->requestSync(request);

    Json::Reader reader;
    bool parsingSuccessful = reader.parse(result.c_str(), data);
    REQUIRE(parsingSuccessful);

    auto status = data["status"].asInt();
    INFO(data["message"].asString());
    CHECK(status == 200);
}
