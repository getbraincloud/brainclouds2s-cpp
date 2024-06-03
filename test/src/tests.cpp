#include "tests.h"
#define CATCH_CONFIG_MAIN

#include <brainclouds2s.h>
#include <brainclouds2s-rtt.h>
#include <IRTTCallback.h>
#include <IRTTConnectCallback.h>
#include <json/json.h>
#include <chrono>

// using namespace std::chrono_literals; // This requires latest CMake on Ubuntu to detect C++ 17 compiler
using namespace BrainCloud;
std::string BRAINCLOUD_APP_ID = "";
std::string BRAINCLOUD_SERVER_NAME = "";
std::string BRAINCLOUD_SERVER_SECRET = "";
std::string BRAINCLOUD_SERVER_URL = "";

bool idsLoaded = false;

void loadIdsIfNot()
{
    if (idsLoaded) return;

    FILE * fp = fopen("ids.txt", "r");
    if (fp == NULL)
    {
        printf("ERROR: Failed to load ids.txt file!\n");
        exit(1);
    }
    else
    {
        printf("Found ids.txt file!\n");
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            char *c = strchr(buf, '\n');
            if (c) { *c = 0; }

            c = strchr(buf, '\r');
            if (c) { *c = 0; }

            std::string line(buf);
            if (line.find("appId") != std::string::npos)
            {
                BRAINCLOUD_APP_ID = line.substr(line.find("appId") + sizeof("appId"), line.length() - 1);
            }
            else if (line.find("serverName") != std::string::npos)
            {
                BRAINCLOUD_SERVER_NAME = line.substr(line.find("serverName") + sizeof("serverName"), line.length() - 1);
            }
            else if (line.find("serverSecret") != std::string::npos)
            {
                BRAINCLOUD_SERVER_SECRET = line.substr(line.find("serverSecret") + sizeof("serverSecret"), line.length() - 1);
            }
            else if (line.find("s2sUrl") != std::string::npos)
            {
                BRAINCLOUD_SERVER_URL = line.substr(line.find("s2sUrl") + sizeof("s2sUrl"), line.length() - 1);
            }
        }
        fclose(fp);

        printf("\nApp ID - %s", BRAINCLOUD_APP_ID.c_str());
        printf("\nserverName - %s", BRAINCLOUD_SERVER_NAME.c_str());
        printf("\nserverSecret - %s", BRAINCLOUD_SERVER_SECRET.c_str());
        printf("\ns2sUrl - %s", BRAINCLOUD_SERVER_URL.c_str());
    }

    if (BRAINCLOUD_APP_ID.empty() ||
        BRAINCLOUD_SERVER_NAME.empty() ||
        BRAINCLOUD_SERVER_SECRET.empty() ||
        BRAINCLOUD_SERVER_URL.empty())
    {
        printf("ERROR: ids.txt missing S2S properties!\n");
        exit(1);
    }

    idsLoaded = true;
}

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

