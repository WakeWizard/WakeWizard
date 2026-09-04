#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "Config.h"
#include "DeviceStore.h"
#include "Logger.h"
#include "Network.h"
#include "WakeEngine.h"
#include "WebApp.h"

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;


namespace
{
    void printNetworkStatus()
    {
        Serial.println();
        Serial.println("================ WakeWizard ================");

        if (Network.isSetupMode())
        {
            Serial.println("MODE: INITIAL SETUP");

            Serial.print("SETUP SSID: ");
            Serial.println(Network.getSetupSSID());

            Serial.print("SETUP PASSWORD: ");
            Serial.println(Network.getSetupPassword());

            Serial.print("SETUP URL: http://");
            Serial.print(Network.getSetupIPAddress());
            Serial.println("/");
        }
        else
        {
            Serial.println("MODE: NORMAL");

            Serial.print("HOSTNAME: ");
            Serial.println(Config.getHostname());

            Serial.print("SSID: ");
            Serial.println(Network.getSSID());

            Serial.print("IP: ");
            Serial.println(Network.getIPAddress());

            Serial.print("MAC: ");
            Serial.println(Network.getMACAddress());

            Serial.print("RSSI: ");
            Serial.print(Network.getRSSI());
            Serial.println(" dBm");

            Serial.print("URL: http://");
            Serial.print(Network.getIPAddress());
            Serial.println("/");

            if (
                Network.getMdnsHostname().length() > 0
            )
            {
                Serial.print("mDNS URL: http://");
                Serial.print(Network.getMdnsHostname());
                Serial.println("/");
            }
        }

        Serial.println("==========================================");
        Serial.println();
    }
}


void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    if (!LittleFS.begin(false))
    {
        Serial.println("LittleFS initialization failed");
        return;
    }

    if (!Config.begin())
    {
        Serial.println("Config initialization failed");
        return;
    }

    if (!Logger.begin())
    {
        Serial.println("Logger initialization failed");
        return;
    }

    Logger.info("System", "boot started");

    bool stationConnected = false;

    if (
        Config.isProvisioned() &&
        strlen(Config.getWifiSSID()) > 0
    )
    {
        stationConnected =
            Network.begin(
                Config.getWifiSSID(),
                Config.getWifiPassword(),
                Config.getHostname(),
                WIFI_TIMEOUT_MS
            );
    }

    if (!stationConnected)
    {
        if (!Network.startSetupAccessPoint())
        {
            Logger.error(
                "Network",
                "unable to start setup access point"
            );
            return;
        }

        Logger.warn(
            "Network",
            "setup access point started; SSID=" +
            Network.getSetupSSID() +
            "; IP=" +
            Network.getSetupIPAddress()
        );
    }
    else
    {
        Logger.startTimeSync();

        const bool mdnsStarted =
            Network.startMdns(
                Config.getHostname()
            );

        Logger.info(
            "System",
            mdnsStarted
                ? "mDNS started; host=" +
                  Network.getMdnsHostname()
                : "mDNS start failed"
        );

        Logger.info(
            "System",
            "WiFi connected; IP=" +
            Network.getIPAddress() +
            "; MAC=" +
            Network.getMACAddress() +
            "; RSSI=" +
            String(Network.getRSSI()) +
            " dBm"
        );
    }

    // Always print how to reach WakeWizard after every restart/upload.
    printNetworkStatus();

    if (!WebApp.begin())
    {
        Logger.error("WebApp", "initialization failed");
        return;
    }

    if (!DeviceStore.begin())
    {
        Logger.error("DeviceStore", "initialization failed");
        return;
    }

    Logger.info(
        "DeviceStore",
        "loaded " +
        String(DeviceStore.getDeviceCount()) +
        " configured device(s)"
    );

    if (!WakeEngine.begin())
    {
        Logger.error("WakeEngine", "initialization failed");
        return;
    }

    if (stationConnected)
    {
        Logger.info("AutoWake", "WiFi and device store ready");

        const size_t autoWakeCount =
            WakeEngine.scheduleWakeOnBoot(
                DeviceStore.getDevices(),
                DeviceStore.getDeviceCount()
            );

        Logger.info(
            "AutoWake",
            "startup scheduling complete; jobs=" +
            String(autoWakeCount)
        );
    }
    else
    {
        Logger.info(
            "AutoWake",
            "skipped while WakeWizard is in setup mode"
        );
    }
}


void loop()
{
    static uint32_t bootPressedAt = 0;
    static bool resetTriggered = false;

    pinMode(0, INPUT_PULLUP);

    if (digitalRead(0) == LOW)
    {
        if (bootPressedAt == 0)
        {
            bootPressedAt = millis();
        }

        if (
            !resetTriggered &&
            millis() - bootPressedAt >= 10000
        )
        {
            resetTriggered = true;

            Logger.warn(
                "System",
                "BOOT held 10 seconds; factory reset"
            );

            const bool devicesCleared =

                DeviceStore.clear();

            const bool configCleared =

                Config.factoryReset();

            if (

                !devicesCleared ||

                !configCleared

            )

            {

                Logger.error(

                    "System",

                    "hardware factory reset failed"

                );

            }

            else

            {

                Logger.warn(

                    "System",

                    "hardware factory reset completed"

                );

            }

            if (configCleared)
            {
                delay(250);

                ESP.restart();
            }
        }
    }
    else
    {
        bootPressedAt = 0;
        resetTriggered = false;
    }

    WebApp.loop();
    WakeEngine.loop();
    Logger.loop();
}
