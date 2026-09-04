#include "Network.h"

#include <ESPmDNS.h>

namespace
{
    constexpr const char* SETUP_AP_PREFIX =
        "WakeWizard-";

    constexpr const char* SETUP_AP_PASSWORD =
        "wakewizard-setup";
}

NetworkClass Network;


bool NetworkClass::begin(
    const char* ssid,
    const char* password,
    const char* hostname,
    uint32_t timeoutMs
)
{
    setupMode = false;
    mdnsHostname = "";

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    if (
        hostname != nullptr &&
        strlen(hostname) > 0
    )
    {
        WiFi.setHostname(
            hostname
        );
    }

    WiFi.begin(
        ssid,
        password
    );

    const uint32_t startTime =
        millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < timeoutMs
    )
    {
        delay(250);
    }

    return
        WiFi.status() ==
        WL_CONNECTED;
}


bool NetworkClass::startSetupAccessPoint()
{
    MDNS.end();

    WiFi.disconnect(true);
    delay(150);

    /*
     * AP+STA keeps scanning available while the browser is connected
     * to the WakeWizard setup network.
     */
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    String suffix =
        WiFi.macAddress();

    suffix.replace(
        ":",
        ""
    );

    if (suffix.length() > 4)
    {
        suffix =
            suffix.substring(
                suffix.length() - 4
            );
    }

    suffix.toUpperCase();

    setupSSID =
        String(SETUP_AP_PREFIX) +
        suffix;

    setupMode =
        WiFi.softAP(
            setupSSID.c_str(),
            SETUP_AP_PASSWORD
        );

    mdnsHostname = "";

    return setupMode;
}


bool NetworkClass::startMdns(
    const char* hostname
)
{
    if (
        setupMode ||
        hostname == nullptr ||
        strlen(hostname) == 0
    )
    {
        return false;
    }

    MDNS.end();

    if (!MDNS.begin(hostname))
    {
        mdnsHostname = "";
        return false;
    }

    MDNS.addService(
        "http",
        "tcp",
        80
    );

    mdnsHostname =
        String(hostname) +
        ".local";

    return true;
}


bool NetworkClass::isConnected() const
{
    return
        WiFi.status() ==
        WL_CONNECTED;
}


bool NetworkClass::isSetupMode() const
{
    return setupMode;
}


String NetworkClass::getSSID() const
{
    return
        setupMode
            ? setupSSID
            : WiFi.SSID();
}


String NetworkClass::getIPAddress() const
{
    return
        setupMode
            ? WiFi.softAPIP().toString()
            : WiFi.localIP().toString();
}


String NetworkClass::getMACAddress() const
{
    return WiFi.macAddress();
}


int32_t NetworkClass::getRSSI() const
{
    return
        setupMode
            ? 0
            : WiFi.RSSI();
}


String NetworkClass::getSetupSSID() const
{
    return setupSSID;
}


String NetworkClass::getSetupIPAddress() const
{
    return
        WiFi.softAPIP().toString();
}


const char* NetworkClass::getSetupPassword() const
{
    return SETUP_AP_PASSWORD;
}


String NetworkClass::getMdnsHostname() const
{
    return mdnsHostname;
}
