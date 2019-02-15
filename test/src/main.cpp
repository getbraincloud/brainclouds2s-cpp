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

int main(int argc, char** argv)
{
    auto runScript = "{ \
        \"service\": \"script\", \
        \"operation\": \"RUN\", \
        \"data\": { \
            \"scriptName\": \"testScript2\" \
        } \
    }";

    int failCount = 0;

    {
        auto pContext = S2SContext::create(
            BRAINCLOUD_APP_ID,
            BRAINCLOUD_SERVER_NAME,
            BRAINCLOUD_SERVER_SECRET,
            BRAINCLOUD_SERVER_URL
        );
        pContext->setLogEnabled(true);

        // Queue many at once
        if (!run(runScript, pContext)) ++failCount;
        if (!run(runScript, pContext)) ++failCount;
        if (!run(runScript, pContext)) ++failCount;
    }

    {
        auto pContext = S2SContext::create(
            BRAINCLOUD_APP_ID,
            BRAINCLOUD_SERVER_NAME,
            BRAINCLOUD_SERVER_SECRET,
            BRAINCLOUD_SERVER_URL
        );
        pContext->setLogEnabled(true);

        // Set some timeout
        if (!run(runScript, pContext)) ++failCount;
        wait(pContext);
    }

    return failCount;
}
