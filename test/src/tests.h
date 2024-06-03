//
// Created by Joanne Hoar on 30/05/2024.
//

#pragma once
#include <brainclouds2s.h>
#include <brainclouds2s-rtt.h>
#include <IRTTCallback.h>
#include <IRTTConnectCallback.h>
#include <json/json.h>
#include <chrono>

extern std::string BRAINCLOUD_APP_ID;
extern std::string BRAINCLOUD_SERVER_NAME;
extern std::string BRAINCLOUD_SERVER_SECRET;
extern std::string BRAINCLOUD_SERVER_URL;

#include "tests.h"

// using namespace std::chrono_literals; // This requires latest CMake on Ubuntu to detect C++ 17 compiler
using namespace BrainCloud;

extern bool idsLoaded;

void loadIdsIfNot();

bool runAuth(S2SContextRef pContext);

bool run(const std::string& request, S2SContextRef pContext);

void wait(S2SContextRef pContext);

