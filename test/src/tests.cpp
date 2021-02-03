#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "ids.h"

#include <brainclouds2s.h>
#include <json/json.h>
#include <chrono>

using namespace std::chrono_literals;

bool runAuth(S2SContextRef pContext)
{
    bool processed = false;
    std::string ret;

    pContext->authenticate([&](const std::string& result)
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






///////////////////////////////////////////////////////////////////////////////
// We start with all tests, but using Auto-Auth.
///////////////////////////////////////////////////////////////////////////////






TEST_CASE("Create context - Auto auth", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        true
    );
    REQUIRE(pContext);

    auto pAnotherContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        true
    );
    REQUIRE(pAnotherContext);
    REQUIRE_FALSE(pContext == pAnotherContext);

    pContext = nullptr;
    pAnotherContext = nullptr;
}

TEST_CASE("Valid Context - Auto auth", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        true
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
        
        auto start_time = std::chrono::system_clock::now();
        while (processed_count < 5)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
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

TEST_CASE("Context with bad Server Secret - Auto auth", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        "null",
        BRAINCLOUD_SERVER_URL,
        true
    );
    REQUIRE(pContext);

    pContext->setLogEnabled(true);

    SECTION("Valid request")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        auto ret = run(request, pContext); // Should fail on auth
        REQUIRE_FALSE(ret);
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
        
        auto start_time = std::chrono::system_clock::now();
        while (processed_count < 5)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
        }

        REQUIRE(success_count == 0);
        REQUIRE(processed_count == 5); // They should all trigger the error
    }
}

TEST_CASE("RunCallbacks with timeout - Auto auth", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        true
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

TEST_CASE("requestSync - Auto auth", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        true
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

TEST_CASE("Bad Requests - Auto auth", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        true
    );
    pContext->setLogEnabled(true);

    SECTION("Single Bad Request")
    {
        auto bad_request = "{ \
            \"service\": \"timey\", \
            \"operation\": \"READ_MUH_TIME\", \
            \"data\": {} \
        }";

        auto ret = run(bad_request, pContext);
        REQUIRE_FALSE(ret);
    }

    SECTION("Queued requests but of of them is bad")
    {
        auto bad_request = "{ \
            \"service\": \"timey\", \
            \"operation\": \"READ_MUH_TIME\", \
            \"data\": {} \
        }";
        auto good_request = "{ \
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
        pContext->request(good_request, callback);
        pContext->request(good_request, callback);
        pContext->request(bad_request, callback);
        pContext->request(good_request, callback);
        pContext->request(good_request, callback);
        
        auto start_time = std::chrono::system_clock::now();
        while (processed_count < 5)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
        }

        REQUIRE(success_count == 4);
    }
}






///////////////////////////////////////////////////////////////////////////////
// Same as above, but Auto-Auth is false.
///////////////////////////////////////////////////////////////////////////////







TEST_CASE("Create context", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
    );
    REQUIRE(pContext);

    auto pAnotherContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
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
        BRAINCLOUD_SERVER_URL,
        false
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

        auto authRet = runAuth(pContext);
        REQUIRE(authRet);

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
        pContext->authenticate(callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        
        auto start_time = std::chrono::system_clock::now();
        while (processed_count < 6)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
        }

        REQUIRE(success_count == 6);
    }

    SECTION("Bad Request Json")
    {
        auto authRet = runAuth(pContext);
        REQUIRE(authRet);

        auto request = "Bad Request";
        auto ret = run(request, pContext);
        REQUIRE_FALSE(ret);
    }

    SECTION("Bad Request")
    {
        auto authRet = runAuth(pContext);
        REQUIRE(authRet);

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
        BRAINCLOUD_SERVER_URL,
        false
    );
    REQUIRE(pContext);

    pContext->setLogEnabled(true);

    SECTION("Valid request")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        auto authRet = runAuth(pContext); // Should fail
        REQUIRE_FALSE(authRet);

        auto ret = run(request, pContext); // Should fail because not auth
        REQUIRE_FALSE(ret);
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
        pContext->authenticate(callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        pContext->request(request, callback);
        
        auto start_time = std::chrono::system_clock::now();
        while (processed_count < 6)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
        }

        REQUIRE(success_count == 0);
        REQUIRE(processed_count == 6); // They should all trigger the error
    }
}

TEST_CASE("RunCallbacks with timeout", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
    );

    pContext->setLogEnabled(true);

    auto request = "{ \
        \"service\": \"time\", \
        \"operation\": \"READ\", \
        \"data\": {} \
    }";

    // Auth
    {
        Json::Value data;
        std::string ret;

        bool processed = false;

        pContext->authenticate([&](const std::string& result)
        {
            ret = result;
            processed = true;
        });

        pContext->runCallbacks(20 * 1000);
        REQUIRE(processed);

        Json::Reader reader;
        bool parsingSuccessful = reader.parse(ret.c_str(), data);
        REQUIRE(parsingSuccessful);

        auto status_code = data["status"].asInt();
        REQUIRE(status_code == 200);
    }

    // The request
    {
        Json::Value data;
        std::string ret;

        bool processed = false;
        
        pContext->request(request, [&](const std::string& result)
        {
            ret = result;
            processed = true;
        });

        pContext->runCallbacks(20 * 1000);
        REQUIRE(processed);

        Json::Reader reader;
        bool parsingSuccessful = reader.parse(ret.c_str(), data);
        REQUIRE(parsingSuccessful);

        auto status_code = data["status"].asInt();
        REQUIRE(status_code == 200);
    }
}

TEST_CASE("requestSync", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
    );

    pContext->setLogEnabled(true);

    SECTION("async auth - sync request")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        Json::Value data;
        bool processed = false;

        auto authRet = runAuth(pContext);
        REQUIRE(authRet);

        std::string result = pContext->requestSync(request);

        Json::Reader reader;
        bool parsingSuccessful = reader.parse(result.c_str(), data);
        REQUIRE(parsingSuccessful);

        auto status = data["status"].asInt();
        INFO(data["message"].asString());
        CHECK(status == 200);
    }

    SECTION("sync auth - sync request")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        Json::Value data;
        bool processed = false;

        // auth
        {
            std::string result = pContext->authenticateSync();

            Json::Reader reader;
            bool parsingSuccessful = reader.parse(result.c_str(), data);
            REQUIRE(parsingSuccessful);

            auto status = data["status"].asInt();
            INFO(data["message"].asString());
            CHECK(status == 200);
        }

        // request
        {
            std::string result = pContext->requestSync(request);

            Json::Reader reader;
            bool parsingSuccessful = reader.parse(result.c_str(), data);
            REQUIRE(parsingSuccessful);

            auto status = data["status"].asInt();
            INFO(data["message"].asString());
            CHECK(status == 200);
        }
    }

    SECTION("sync auth - async request")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        Json::Value data;
        bool processed = false;

        // auth
        {
            std::string result = pContext->authenticateSync();

            Json::Reader reader;
            bool parsingSuccessful = reader.parse(result.c_str(), data);
            REQUIRE(parsingSuccessful);

            auto status = data["status"].asInt();
            INFO(data["message"].asString());
            CHECK(status == 200);
        }

        auto ret = run(request, pContext);
        REQUIRE(ret);
    }

    SECTION("async request - sync auth")
    {
        auto request = "{ \
            \"service\": \"time\", \
            \"operation\": \"READ\", \
            \"data\": {} \
        }";

        Json::Value data;
        Json::Value request_data;
        bool processed = false;

        // The request
        {
            pContext->request(request, [&](const std::string& result)
            {
                processed = true;

                Json::Reader reader;
                bool parsingSuccessful = reader.parse(result.c_str(), request_data);
                REQUIRE(parsingSuccessful);

                // This should fail because it was called _before_ auth.
                auto status_code = request_data["status"].asInt();
                REQUIRE_FALSE(status_code == 200);
            });
        }

        // auth
        {
            std::string result = pContext->authenticateSync();

            Json::Reader reader;
            bool parsingSuccessful = reader.parse(result.c_str(), data);
            REQUIRE(parsingSuccessful);

            auto status = data["status"].asInt();
            INFO(data["message"].asString());
            CHECK(status == 200);
        }
        
        // Wait on the request
        auto start_time = std::chrono::system_clock::now();
        while (!processed)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
        }
        REQUIRE(processed);
    }
}

TEST_CASE("Bad Requests", "[S2S]")
{
    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false
    );
    pContext->setLogEnabled(true);

    SECTION("Single Bad Request")
    {
        auto bad_request = "{ \
            \"service\": \"timey\", \
            \"operation\": \"READ_MUH_TIME\", \
            \"data\": {} \
        }";

        auto retAuth = runAuth(pContext);
        REQUIRE(retAuth);

        auto ret = run(bad_request, pContext);
        REQUIRE_FALSE(ret);
    }

    SECTION("Queued requests but of of them is bad")
    {
        auto bad_request = "{ \
            \"service\": \"timey\", \
            \"operation\": \"READ_MUH_TIME\", \
            \"data\": {} \
        }";
        auto good_request = "{ \
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
        pContext->authenticate(callback);
        pContext->request(good_request, callback);
        pContext->request(good_request, callback);
        pContext->request(bad_request, callback);
        pContext->request(good_request, callback);
        pContext->request(good_request, callback);
        
        auto start_time = std::chrono::system_clock::now();
        while (processed_count < 6)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start_time > 20s)
            {
                printf("Timeout");
                break;
            }
        }

        REQUIRE(success_count == 5);
    }
}
