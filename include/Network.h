#pragma once

#include <Arduino.h>
#include <WiFi.h>

class NetworkClass
{
public:
    bool begin(
        const char* ssid,
        const char* password,
        const char* hostname,
        uint32_t timeoutMs = 20000
    );

    bool startSetupAccessPoint();
    bool startMdns(const char* hostname);

    bool isConnected() const;
    bool isSetupMode() const;

    String getSSID() const;
    String getIPAddress() const;
    String getMACAddress() const;
    int32_t getRSSI() const;

    String getSetupSSID() const;
    String getSetupIPAddress() const;
    const char* getSetupPassword() const;

    String getMdnsHostname() const;

private:
    bool setupMode = false;
    String setupSSID;
    String mdnsHostname;
};

extern NetworkClass Network;
