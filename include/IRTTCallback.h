// Copyright 2026 bitHeads, Inc. All Rights Reserved.
//
//  IRTTConnectCallback.h
//  GameClientLib
//
//  Created by David St-Louis on 2018-08-09.
//
#pragma once

#include <string>

namespace BrainCloud {
    
    class IRTTCallback
    {
    public:
        virtual ~IRTTCallback( )  { }
        
        /**
         * Method called on RTT events
         *
         * @param jsonData - Data for the event.
         */
        virtual void rttCallback(const std::string& jsonData) = 0;
    };
    
};


