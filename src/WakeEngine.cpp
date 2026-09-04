#include "WakeEngine.h"

#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "ping/ping_sock.h"
#include "lwip/inet.h"

#include "Logger.h"

namespace
{
    volatile bool pingSuccess = false;
    volatile bool pingFinished = false;

    void onPingSuccess(
        esp_ping_handle_t handle,
        void* args)
    {
        pingSuccess = true;
    }

    void onPingEnd(
        esp_ping_handle_t handle,
        void* args)
    {
        pingFinished = true;
    }
}

WakeEngineClass WakeEngine;


bool WakeEngineClass::begin()
{
    if (initialized)
    {
        return true;
    }

    if (udp.begin(WAKE_SOURCE_PORT) == 0)
    {
        Logger.error("WakeEngine", "UDP initialization failed");
        return false;
    }

    initialized = true;

    Logger.info("WakeEngine", "ready");

    return true;
}


bool WakeEngineClass::wakeManual(
    const Device& device)
{
    if (!initialized && !begin())
    {
        return false;
    }

    // Manual wake is immediate. initialDelayMs is specifically
    // the delay after Wi-Fi becomes available during boot.
    return queue(device, 0);
}


size_t WakeEngineClass::scheduleWakeOnBoot(
    const Device* devices,
    size_t count)
{
    if (!initialized && !begin())
    {
        return 0;
    }

    size_t queuedCount = 0;

    Logger.info(
        "AutoWake",
        "evaluating " +
        String(count) +
        " configured device(s)"
    );

    for (size_t i = 0; i < count; i++)
    {
        const Device& device = devices[i];

        if (!device.enabled)
        {
            Logger.info(
                "AutoWake",
                "skip " + device.name +
                " - disabled"
            );
            continue;
        }

        if (!device.wakeOnBoot)
        {
            Logger.info(
                "AutoWake",
                "skip " + device.name +
                " - wakeOnBoot=false"
            );
            continue;
        }

        Logger.info(
            "AutoWake",
            "schedule " + device.name +
            " [" + String(device.id) +
            "] after " +
            String(device.initialDelayMs) +
            "ms"
        );

        if (queue(
                device,
                device.initialDelayMs
            ))
        {
            queuedCount++;
        }
    }

    Logger.info(
        "AutoWake",
        "queued " + String(queuedCount) +
        " device(s)"
    );

    return queuedCount;
}


bool WakeEngineClass::queue(
    const Device& device,
    uint32_t initialDelayMs)
{
    if (device.packetCount == 0)
    {
        Logger.warn(
            "WakeEngine",
            "packetCount is zero for " + device.name
        );
        return false;
    }

    uint8_t mac[6];

    if (!parseMac(device.mac, mac))
    {
        Logger.error(
            "WakeEngine",
            "invalid MAC for " + device.name
        );
        return false;
    }

    if (device.secureOn.length() > 0)
    {
        uint8_t secureOn[6];

        if (!parseSecureOn(
                device.secureOn,
                secureOn))
        {
            Logger.error(
                "WakeEngine",
                "invalid SecureOn password for " +
                device.name
            );
            return false;
        }
    }

    WakeJob* job = findJob(device.id);

    if (job != nullptr && job->active)
    {
        Logger.warn(
            "WakeEngine",
            "device already queued/active " +
            device.name +
            " [" + String(device.id) + "]"
        );
        return false;
    }

    if (job == nullptr)
    {
        job = findFreeJob();
    }

    if (job == nullptr)
    {
        Logger.error(
            "WakeEngine",
            "no free wake job slots"
        );
        return false;
    }

    const uint32_t now = millis();

    job->active = true;
    job->device = device;
    job->packetsSent = 0;
    job->reachabilityChecks = 0;
    job->nextPacketAtMs = now + initialDelayMs;
    job->nextReachabilityCheckAtMs = now + initialDelayMs;

    Logger.info(
        "WakeEngine",
        "queued " + device.name +
        " [" + String(device.id) +
        "] delay=" +
        String(initialDelayMs) +
        "ms"
    );

    return true;
}


void WakeEngineClass::loop()
{
    if (!initialized || WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    const uint32_t reachabilityNow = millis();

    for (size_t offset = 0; offset < MAX_ACTIVE_WAKES; offset++)
    {
        const size_t i =
            (nextReachabilityJob + offset) %
            MAX_ACTIVE_WAKES;

        WakeJob& job = jobs[i];

        if (!job.active)
        {
            continue;
        }

        const bool canCheckReachability =
            job.device.stopWhenReachable &&
            job.device.ip.length() > 0 &&
            job.device.maxReachabilityChecks > 0 &&
            job.reachabilityChecks < job.device.maxReachabilityChecks;

        if (
            canCheckReachability &&
            timeReached(
                reachabilityNow,
                job.nextReachabilityCheckAtMs
            ))
        {
            nextReachabilityJob =
                (i + 1) % MAX_ACTIVE_WAKES;

            job.reachabilityChecks++;

            if (isReachable(job.device))
            {
                completeJob(
                    job,
                    "target reachable"
                );
            }
            else
            {
                job.nextReachabilityCheckAtMs =
                    millis() + REACHABILITY_INTERVAL_MS;
            }

            break;
        }
    }

    for (size_t i = 0; i < MAX_ACTIVE_WAKES; i++)
    {
        WakeJob& job = jobs[i];

        if (
            job.active &&
            timeReached(
                millis(),
                job.nextPacketAtMs
            ))
        {
            if (!sendMagicPacket(job.device))
            {
                completeJob(
                    job,
                    "send failed"
                );
                continue;
            }

            job.packetsSent++;

            Logger.info(
                "WakeEngine",
                "Magic packet " +
                job.device.name +
                " [" + String(job.device.id) +
                "] " +
                String(job.packetsSent) +
                "/" +
                String(job.device.packetCount)
            );

            if (job.packetsSent >= job.device.packetCount)
            {
                completeJob(
                    job,
                    "packet sequence completed"
                );
                continue;
            }

            job.nextPacketAtMs =
                millis() + job.device.packetIntervalMs;
        }
    }
}


bool WakeEngineClass::isActive(
    uint32_t deviceId) const
{
    for (size_t i = 0; i < MAX_ACTIVE_WAKES; i++)
    {
        if (
            jobs[i].active &&
            jobs[i].device.id == deviceId
        )
        {
            return true;
        }
    }

    return false;
}


WakeEngineClass::WakeJob*
WakeEngineClass::findJob(uint32_t deviceId)
{
    for (size_t i = 0; i < MAX_ACTIVE_WAKES; i++)
    {
        if (
            jobs[i].active &&
            jobs[i].device.id == deviceId
        )
        {
            return &jobs[i];
        }
    }

    return nullptr;
}


WakeEngineClass::WakeJob*
WakeEngineClass::findFreeJob()
{
    for (size_t i = 0; i < MAX_ACTIVE_WAKES; i++)
    {
        if (!jobs[i].active)
        {
            return &jobs[i];
        }
    }

    return nullptr;
}


bool WakeEngineClass::sendMagicPacket(
    const Device& device)
{
    uint8_t mac[6];

    if (!parseMac(device.mac, mac))
    {
        return false;
    }

    constexpr size_t STANDARD_PACKET_SIZE = 102;
    constexpr size_t SECURE_ON_SIZE = 6;

    uint8_t packet[
        STANDARD_PACKET_SIZE +
        SECURE_ON_SIZE
    ];

    size_t packetLength =
        STANDARD_PACKET_SIZE;

    for (size_t i = 0; i < 6; i++)
    {
        packet[i] = 0xFF;
    }

    for (size_t repetition = 0;
         repetition < 16;
         repetition++)
    {
        memcpy(
            &packet[6 + repetition * 6],
            mac,
            6
        );
    }

    if (device.secureOn.length() > 0)
    {
        uint8_t secureOn[SECURE_ON_SIZE];

        if (!parseSecureOn(
                device.secureOn,
                secureOn))
        {
            Logger.error(
                "WakeEngine",
                "invalid SecureOn password for " +
                device.name
            );
            return false;
        }

        memcpy(
            &packet[STANDARD_PACKET_SIZE],
            secureOn,
            SECURE_ON_SIZE
        );

        packetLength +=
            SECURE_ON_SIZE;
    }

    const IPAddress broadcast =
        resolveBroadcast(device);

    if (!udp.beginPacket(
            broadcast,
            device.udpPort))
    {
        return false;
    }

    const size_t written =
        udp.write(
            packet,
            packetLength
        );

    if (written != packetLength)
    {
        return false;
    }

    if (!udp.endPacket())
    {
        return false;
    }

    Logger.info(
        "WakeEngine",
        "UDP " + broadcast.toString() +
        ":" + String(device.udpPort) +
        " length=" + String(packetLength)
    );

    return true;
}


bool WakeEngineClass::isReachable(
    const Device& device)
{
    IPAddress ip;

    if (!ip.fromString(device.ip))
    {
        return false;
    }

    pingSuccess = false;
    pingFinished = false;

    ip_addr_t targetAddress;
    targetAddress.type = IPADDR_TYPE_V4;

    IP4_ADDR(
        &targetAddress.u_addr.ip4,
        ip[0],
        ip[1],
        ip[2],
        ip[3]
    );

    esp_ping_config_t config =
        ESP_PING_DEFAULT_CONFIG();

    config.target_addr = targetAddress;
    config.count = 1;
    config.interval_ms = 10;
    const uint32_t configuredTimeout =
        device.pingTimeoutMs > 0
            ? device.pingTimeoutMs
            : 500;

    config.timeout_ms =
        configuredTimeout < MAX_PING_TIMEOUT_MS
            ? configuredTimeout
            : MAX_PING_TIMEOUT_MS;

    esp_ping_callbacks_t callbacks = {};
    callbacks.on_ping_success = onPingSuccess;
    callbacks.on_ping_end = onPingEnd;

    esp_ping_handle_t pingHandle;

    if (
        esp_ping_new_session(
            &config,
            &callbacks,
            &pingHandle
        ) != ESP_OK
    )
    {
        return false;
    }

    esp_ping_start(pingHandle);

    const uint32_t start = millis();
    const uint32_t waitLimit =
        config.timeout_ms + 150;

    while (
        !pingFinished &&
        millis() - start < waitLimit
    )
    {
        delay(1);
    }

    esp_ping_stop(pingHandle);
    esp_ping_delete_session(pingHandle);

    return pingSuccess;
}


bool WakeEngineClass::parseMac(
    const String& value,
    uint8_t mac[6]) const
{
    unsigned int values[6];

    const int parsed = sscanf(
        value.c_str(),
        "%x:%x:%x:%x:%x:%x",
        &values[0],
        &values[1],
        &values[2],
        &values[3],
        &values[4],
        &values[5]
    );

    if (parsed != 6)
    {
        return false;
    }

    for (size_t i = 0; i < 6; i++)
    {
        if (values[i] > 0xFF)
        {
            return false;
        }

        mac[i] =
            static_cast<uint8_t>(
                values[i]
            );
    }

    return true;
}


bool WakeEngineClass::parseSecureOn(
    const String& value,
    uint8_t password[6]) const
{
    if (value.length() != 17)
    {
        return false;
    }

    for (size_t i = 0; i < value.length(); i++)
    {
        if ((i + 1) % 3 == 0)
        {
            if (value[i] != ':')
            {
                return false;
            }
        }
        else if (!isHexadecimalDigit(value[i]))
        {
            return false;
        }
    }

    return parseMac(value, password);
}


IPAddress WakeEngineClass::resolveBroadcast(
    const Device& device) const
{
    IPAddress configured;

    if (
        device.broadcast.length() > 0 &&
        configured.fromString(device.broadcast)
    )
    {
        return configured;
    }

    const IPAddress local =
        WiFi.localIP();

    const IPAddress mask =
        WiFi.subnetMask();

    return IPAddress(
        local[0] | static_cast<uint8_t>(~mask[0]),
        local[1] | static_cast<uint8_t>(~mask[1]),
        local[2] | static_cast<uint8_t>(~mask[2]),
        local[3] | static_cast<uint8_t>(~mask[3])
    );
}


bool WakeEngineClass::timeReached(
    uint32_t now,
    uint32_t target) const
{
    return static_cast<int32_t>(now - target) >= 0;
}


void WakeEngineClass::completeJob(
    WakeJob& job,
    const char* reason)
{
    Logger.info(
        "WakeEngine",
        "completed " + job.device.name +
        " [" + String(job.device.id) +
        "] - " + String(reason)
    );

    job.active = false;
}
