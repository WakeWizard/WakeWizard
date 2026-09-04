#pragma once

#include <Arduino.h>
#include <WiFi.h>

struct DiscoveredHost
{
    IPAddress ip;
    char mac[18];
};

class NetworkScannerClass
{
public:
    size_t scan(
        DiscoveredHost* results,
        size_t maxResults
    );

    size_t scanRange(
        const IPAddress& startIP,
        const IPAddress& endIP,
        DiscoveredHost* results,
        size_t maxResults
    );

    bool getLocalHostRange(
        IPAddress& startIP,
        IPAddress& endIP
    ) const;

    bool validateRange(
        const IPAddress& startIP,
        const IPAddress& endIP,
        String& errorMessage
    ) const;
};

extern NetworkScannerClass NetworkScanner;