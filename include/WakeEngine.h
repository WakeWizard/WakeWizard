#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

#include "Device.h"

class WakeEngineClass
{
public:
    bool begin();

    bool wakeManual(const Device& device);

    size_t scheduleWakeOnBoot(
        const Device* devices,
        size_t count
    );

    void loop();

    bool isActive(uint32_t deviceId) const;

private:
    static constexpr size_t MAX_ACTIVE_WAKES = 32;
    static constexpr uint32_t REACHABILITY_INTERVAL_MS = 1000;
    static constexpr uint32_t MAX_PING_TIMEOUT_MS = 1000;
    static constexpr uint16_t WAKE_SOURCE_PORT = 40000;

    struct WakeJob
    {
        bool active = false;
        Device device;

        uint16_t packetsSent = 0;
        uint16_t reachabilityChecks = 0;

        uint32_t nextPacketAtMs = 0;
        uint32_t nextReachabilityCheckAtMs = 0;
    };

    WakeJob jobs[MAX_ACTIVE_WAKES];
    WiFiUDP udp;
    bool initialized = false;
    size_t nextReachabilityJob = 0;

    bool queue(
        const Device& device,
        uint32_t initialDelayMs
    );

    WakeJob* findJob(uint32_t deviceId);
    WakeJob* findFreeJob();

    bool sendMagicPacket(const Device& device);
    bool isReachable(const Device& device);

    bool parseMac(
        const String& value,
        uint8_t mac[6]
    ) const;

    bool parseSecureOn(
        const String& value,
        uint8_t password[6]
    ) const;

    IPAddress resolveBroadcast(
        const Device& device
    ) const;

    bool timeReached(
        uint32_t now,
        uint32_t target
    ) const;

    void completeJob(
        WakeJob& job,
        const char* reason
    );
};

extern WakeEngineClass WakeEngine;
