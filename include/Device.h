#pragma once

#include <Arduino.h>

struct Device
{
    uint32_t id;

    bool enabled;
    bool wakeOnBoot;

    String name;
    String mac;
    String ip;

    uint16_t udpPort;

    uint32_t initialDelayMs;
    uint16_t packetCount;
    uint32_t packetIntervalMs;

    bool stopWhenReachable;

    uint16_t maxReachabilityChecks;
    uint32_t pingTimeoutMs;

    String broadcast;
    String secureOn;

    String category;
    String notes;

    uint32_t created;
    uint32_t updated;
};