#include "NetworkScanner.h"

#include <WiFi.h>

#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"

#include "esp_netif.h"
#include "esp_netif_net_stack.h"

#include "Logger.h"
namespace
{
    volatile bool pingSuccess = false;
    volatile bool pingFinished = false;

    void onPingSuccess(
        esp_ping_handle_t hdl,
        void* args)
    {
        pingSuccess = true;
    }

    void onPingEnd(
        esp_ping_handle_t hdl,
        void* args)
    {
        pingFinished = true;
    }

    uint32_t ipToNumber(
        const IPAddress& ip)
    {
        return
            (static_cast<uint32_t>(ip[0]) << 24) |
            (static_cast<uint32_t>(ip[1]) << 16) |
            (static_cast<uint32_t>(ip[2]) << 8) |
            static_cast<uint32_t>(ip[3]);
    }


    IPAddress numberToIp(
        uint32_t value)
    {
        return IPAddress(
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        );
    }


    bool pingHost(IPAddress ip)
    {
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
        config.timeout_ms = 120;

        esp_ping_callbacks_t callbacks = {};

        callbacks.on_ping_success =
            onPingSuccess;

        callbacks.on_ping_end =
            onPingEnd;

        esp_ping_handle_t pingHandle;

        if (esp_ping_new_session(
                &config,
                &callbacks,
                &pingHandle) != ESP_OK)
        {
            return false;
        }

        esp_ping_start(pingHandle);

        const uint32_t start =
            millis();

        while (
            !pingFinished &&
            millis() - start < 250)
        {
            delay(1);
        }

        esp_ping_stop(pingHandle);
        esp_ping_delete_session(pingHandle);

        return pingSuccess;
    }


    bool getMacFromArp(
        IPAddress ip,
        char* macBuffer,
        size_t bufferSize)
    {
        esp_netif_t* espNetif =
            esp_netif_get_handle_from_ifkey(
                "WIFI_STA_DEF"
            );

        if (espNetif == nullptr)
        {
            return false;
        }

        const int netifIndex =
            esp_netif_get_netif_impl_index(
                espNetif
            );

        if (netifIndex < 0)
        {
            return false;
        }

        struct netif* lwipNetif =
            netif_get_by_index(
                netifIndex
            );

        if (lwipNetif == nullptr)
        {
            return false;
        }

        ip4_addr_t targetIP;

        IP4_ADDR(
            &targetIP,
            ip[0],
            ip[1],
            ip[2],
            ip[3]
        );

        struct eth_addr* ethAddress =
            nullptr;

        const ip4_addr_t* resolvedIP =
            nullptr;

        const s8_t arpIndex =
            etharp_find_addr(
                lwipNetif,
                &targetIP,
                &ethAddress,
                &resolvedIP
            );

        if (
            arpIndex < 0 ||
            ethAddress == nullptr)
        {
            return false;
        }

        snprintf(
            macBuffer,
            bufferSize,
            "%02X:%02X:%02X:%02X:%02X:%02X",
            ethAddress->addr[0],
            ethAddress->addr[1],
            ethAddress->addr[2],
            ethAddress->addr[3],
            ethAddress->addr[4],
            ethAddress->addr[5]
        );

        return true;
    }
}


NetworkScannerClass NetworkScanner;


bool NetworkScannerClass::getLocalHostRange(
    IPAddress& startIP,
    IPAddress& endIP) const
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    const uint32_t local =
        ipToNumber(
            WiFi.localIP()
        );

    const uint32_t mask =
        ipToNumber(
            WiFi.subnetMask()
        );

    const uint32_t network =
        local & mask;

    const uint32_t broadcast =
        network | (~mask);

    if (
        broadcast <= network + 1
    )
    {
        return false;
    }

    startIP =
        numberToIp(
            network + 1
        );

    endIP =
        numberToIp(
            broadcast - 1
        );

    return true;
}


bool NetworkScannerClass::validateRange(
    const IPAddress& startIP,
    const IPAddress& endIP,
    String& errorMessage) const
{
    if (WiFi.status() != WL_CONNECTED)
    {
        errorMessage =
            "Wi-Fi is not connected";
        return false;
    }

    const uint32_t local =
        ipToNumber(
            WiFi.localIP()
        );

    const uint32_t mask =
        ipToNumber(
            WiFi.subnetMask()
        );

    const uint32_t network =
        local & mask;

    const uint32_t broadcast =
        network | (~mask);

    const uint32_t start =
        ipToNumber(startIP);

    const uint32_t end =
        ipToNumber(endIP);

    if (
        (start & mask) != network ||
        (end & mask) != network
    )
    {
        errorMessage =
            "Scan range must be inside the local subnet";
        return false;
    }

    if (
        start <= network ||
        start >= broadcast ||
        end <= network ||
        end >= broadcast
    )
    {
        errorMessage =
            "Network and broadcast addresses cannot be scanned";
        return false;
    }

    if (start > end)
    {
        errorMessage =
            "Start IP must not be greater than End IP";
        return false;
    }

    constexpr uint32_t MAX_SCAN_ADDRESSES =
        254;

    const uint32_t count =
        end - start + 1;

    if (count > MAX_SCAN_ADDRESSES)
    {
        errorMessage =
            "Scan range is limited to 254 addresses";
        return false;
    }

    errorMessage = "";
    return true;
}


size_t NetworkScannerClass::scan(
    DiscoveredHost* results,
    size_t maxResults)
{
    IPAddress startIP;
    IPAddress endIP;

    if (
        !getLocalHostRange(
            startIP,
            endIP
        )
    )
    {
        return 0;
    }

    /*
     * Keep the normal Scan Network operation bounded to the same maximum
     * number of host addresses the original implementation scanned.
     */
    const uint32_t start =
        ipToNumber(startIP);

    uint32_t end =
        ipToNumber(endIP);

    if (
        end - start + 1 >
        254
    )
    {
        end =
            start + 253;

        endIP =
            numberToIp(end);
    }

    return scanRange(
        startIP,
        endIP,
        results,
        maxResults
    );
}


size_t NetworkScannerClass::scanRange(
    const IPAddress& startIP,
    const IPAddress& endIP,
    DiscoveredHost* results,
    size_t maxResults)
{
    String validationError;

    if (
        !validateRange(
            startIP,
            endIP,
            validationError
        )
    )
    {
        return 0;
    }

    const IPAddress localIP =
        WiFi.localIP();

    const uint32_t start =
        ipToNumber(startIP);

    const uint32_t end =
        ipToNumber(endIP);

    size_t found = 0;

    for (
        uint32_t address = start;
        address <= end;
        ++address)
    {
        const IPAddress candidate =
            numberToIp(address);

        if (candidate == localIP)
        {
            continue;
        }

        // Touch the host so lwIP can populate ARP.
        pingHost(candidate);

        delay(2);

        char mac[18] = "";

        if (
            getMacFromArp(
                candidate,
                mac,
                sizeof(mac)))
        {
            Logger.info(
                "NetworkScanner",
                "FOUND: " +
                candidate.toString() +
                "  " +
                String(mac)
            );

            if (found < maxResults)
            {
                results[found].ip =
                    candidate;

                strncpy(
                    results[found].mac,
                    mac,
                    sizeof(results[found].mac)
                );

                results[found]
                    .mac[
                        sizeof(
                            results[found].mac
                        ) - 1
                    ] = '\0';

                found++;
            }
        }

        /*
         * Guard against uint32_t wraparound even though validated ranges
         * cannot currently reach 255.255.255.255 on a normal LAN.
         */
        if (address == end)
        {
            break;
        }
    }

    return found;
}
