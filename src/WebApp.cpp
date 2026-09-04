#include "WebApp.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_system.h>
#include <ESP.h>

#include "Config.h"
#include "DeviceStore.h"
#include "Logger.h"
#include "Network.h"
#include "NetworkScanner.h"
#include "WakeEngine.h"

namespace
{
    WebServer server(80);

    uint32_t restartRequestedAt = 0;

    void sendJsonError(
        int statusCode,
        const char* message
    );

    void sendJsonSuccess(
        int statusCode = 200
    );


    constexpr uint32_t SESSION_IDLE_TIMEOUT_MS =
        12UL * 60UL * 60UL * 1000UL;

    constexpr uint32_t LOGIN_FAILURE_DELAY_MS =
        1000UL;

    String sessionToken;
    uint32_t sessionLastActivity = 0;
    uint32_t lastFailedLoginAt = 0;


    String randomSessionToken()
    {
        static const char* HEX_CHARS =
            "0123456789abcdef";

        String token;
        token.reserve(64);

        for (int i = 0; i < 32; ++i)
        {
            const uint8_t value =
                static_cast<uint8_t>(
                    esp_random() & 0xFF
                );

            token +=
                HEX_CHARS[
                    (value >> 4) & 0x0F
                ];

            token +=
                HEX_CHARS[
                    value & 0x0F
                ];
        }

        return token;
    }


    String getCookieValue(
        const String& cookieHeader,
        const String& name
    )
    {
        const String prefix =
            name + "=";

        int start = 0;

        while (start < cookieHeader.length())
        {
            int end =
                cookieHeader.indexOf(
                    ';',
                    start
                );

            if (end < 0)
            {
                end =
                    cookieHeader.length();
            }

            String part =
                cookieHeader.substring(
                    start,
                    end
                );

            part.trim();

            if (part.startsWith(prefix))
            {
                return part.substring(
                    prefix.length()
                );
            }

            start = end + 1;
        }

        return "";
    }


    bool sessionExpired()
    {
        if (sessionToken.length() == 0)
        {
            return true;
        }

        return
            static_cast<uint32_t>(
                millis() -
                sessionLastActivity
            ) >
            SESSION_IDLE_TIMEOUT_MS;
    }


    bool isAuthenticated()
    {
        if (!Config.isProvisioned())
        {
            return false;
        }

        if (sessionExpired())
        {
            sessionToken = "";
            return false;
        }

        const String cookie =
            server.header("Cookie");

        const String candidate =
            getCookieValue(
                cookie,
                "WWSESSION"
            );

        if (
            candidate.length() == 0 ||
            candidate != sessionToken
        )
        {
            return false;
        }

        sessionLastActivity =
            millis();

        return true;
    }


    bool ensureAuthenticated()
    {
        if (isAuthenticated())
        {
            return true;
        }

        sendJsonError(
            401,
            "Authentication required"
        );

        return false;
    }


    bool ensureSetupOrAuthenticated()
    {
        if (!Config.isProvisioned())
        {
            return true;
        }

        return ensureAuthenticated();
    }


    void clearSession()
    {
        sessionToken = "";

        server.sendHeader(
            "Set-Cookie",
            "WWSESSION=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict"
        );
    }


    void handleAuthStatusApi()
    {
        JsonDocument doc;

        doc["authRequired"] =
            Config.isProvisioned();

        doc["authenticated"] =
            isAuthenticated();

        doc["setupMode"] =
            !Config.isProvisioned();

        String json;
        serializeJson(doc, json);

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleAuthLoginApi()
    {
        if (!Config.isProvisioned())
        {
            sendJsonSuccess();
            return;
        }

        const uint32_t sinceFailure =
            millis() -
            lastFailedLoginAt;

        if (
            lastFailedLoginAt != 0 &&
            sinceFailure <
            LOGIN_FAILURE_DELAY_MS
        )
        {
            sendJsonError(
                429,
                "Please wait before trying again"
            );
            return;
        }

        JsonDocument doc;

        if (
            deserializeJson(
                doc,
                server.arg("plain")
            )
        )
        {
            sendJsonError(
                400,
                "Invalid login request"
            );
            return;
        }

        const String password =
            doc["password"] | "";

        if (
            !Config.verifyAdminPassword(
                password
            )
        )
        {
            lastFailedLoginAt =
                millis();

            Logger.warn(
                "Auth",
                "failed administrator login"
            );

            sendJsonError(
                401,
                "Invalid administrator password"
            );
            return;
        }

        sessionToken =
            randomSessionToken();

        sessionLastActivity =
            millis();

        server.sendHeader(
            "Set-Cookie",
            "WWSESSION=" +
            sessionToken +
            "; Path=/; HttpOnly; SameSite=Strict"
        );

        Logger.info(
            "Auth",
            "administrator login successful"
        );

        sendJsonSuccess();
    }


    void handleAuthLogoutApi()
    {
        const bool wasAuthenticated =
            isAuthenticated();

        clearSession();

        if (wasAuthenticated)
        {
            Logger.info(
                "Auth",
                "administrator logged out"
            );
        }

        sendJsonSuccess();
    }



    void sendJsonError(
        int statusCode,
        const char* message)
    {
        JsonDocument doc;

        doc["success"] = false;
        doc["message"] = message;

        String json;
        serializeJson(doc, json);

        server.send(
            statusCode,
            "application/json",
            json
        );
    }


    void sendJsonSuccess(int statusCode)
    {
        JsonDocument doc;
        doc["success"] = true;

        String json;
        serializeJson(doc, json);

        server.send(
            statusCode,
            "application/json",
            json
        );
    }


    bool extractDeviceId(
        const String& uri,
        uint32_t& id)
    {
        const String prefix = "/api/devices/";

        if (!uri.startsWith(prefix))
        {
            return false;
        }

        const String idText =
            uri.substring(prefix.length());

        if (idText.length() == 0)
        {
            return false;
        }

        for (size_t i = 0; i < idText.length(); i++)
        {
            if (!isDigit(idText[i]))
            {
                return false;
            }
        }

        id = static_cast<uint32_t>(idText.toInt());

        return id > 0;
    }




    bool extractWakeDeviceId(
        const String& uri,
        uint32_t& id)
    {
        const String prefix = "/api/devices/";
        const String suffix = "/wake";

        if (
            !uri.startsWith(prefix) ||
            !uri.endsWith(suffix)
        )
        {
            return false;
        }

        const size_t idStart = prefix.length();
        const size_t idLength =
            uri.length() - prefix.length() - suffix.length();

        if (idLength == 0)
        {
            return false;
        }

        const String idText =
            uri.substring(
                idStart,
                idStart + idLength
            );

        for (size_t i = 0; i < idText.length(); i++)
        {
            if (!isDigit(idText[i]))
            {
                return false;
            }
        }

        id = static_cast<uint32_t>(idText.toInt());

        return id > 0;
    }

    bool fillDeviceFromRequest(
        Device& device,
        bool preserveMetadata)
    {
        if (!server.hasArg("plain"))
        {
            sendJsonError(
                400,
                "Missing request body"
            );

            return false;
        }

        JsonDocument doc;

        const DeserializationError error =
            deserializeJson(
                doc,
                server.arg("plain")
            );

        if (error)
        {
            sendJsonError(
                400,
                "Invalid JSON"
            );

            return false;
        }

        const uint32_t existingId = device.id;
        const uint32_t existingCreated = device.created;

        device.name =
            doc["name"] | "";

        device.mac =
            doc["mac"] | "";

        device.ip =
            doc["ip"] | "";

        device.enabled =
            doc["enabled"] | true;

        device.wakeOnBoot =
            doc["wakeOnBoot"] | true;

        device.udpPort =
            doc["udpPort"] | 9;

        const uint32_t initialDelaySec =
            doc["initialDelaySec"] | 30;

        device.initialDelayMs =
            initialDelaySec * 1000UL;

        device.packetCount =
            doc["packetCount"] | 5;

        const uint32_t packetIntervalSec =
            doc["packetIntervalSec"] | 30;

        device.packetIntervalMs =
            packetIntervalSec * 1000UL;

        device.stopWhenReachable =
            doc["stopWhenReachable"] | true;

        device.maxReachabilityChecks =
            doc["maxReachabilityChecks"] | 20;

        device.pingTimeoutMs =
            doc["pingTimeoutMs"] | 500;

        device.broadcast =
            doc["broadcast"] | "";

        device.secureOn =
            doc["secureOn"] | "";

        device.category =
            doc["category"] | "";

        device.notes =
            doc["notes"] | "";

        if (preserveMetadata)
        {
            device.id = existingId;
            device.created = existingCreated;
        }
        else
        {
            device.id = 0;
            device.created = 0;
        }

        // RTC/NTP is not implemented yet.
        device.updated = 0;

        String validationError;

        if (!DeviceStore.validateDevice(
                device,
                validationError))
        {
            sendJsonError(
                400,
                validationError.c_str()
            );

            return false;
        }

        return true;
    }


    void addDeviceToJson(
        JsonObject obj,
        const Device& device)
    {
        obj["id"] = device.id;
        obj["name"] = device.name;
        obj["mac"] = device.mac;
        obj["ip"] = device.ip;
        obj["enabled"] = device.enabled;
        obj["wakeOnBoot"] = device.wakeOnBoot;
        obj["udpPort"] = device.udpPort;
        obj["initialDelayMs"] = device.initialDelayMs;
        obj["packetCount"] = device.packetCount;
        obj["packetIntervalMs"] = device.packetIntervalMs;
        obj["stopWhenReachable"] = device.stopWhenReachable;
        obj["maxReachabilityChecks"] = device.maxReachabilityChecks;
        obj["pingTimeoutMs"] = device.pingTimeoutMs;
        obj["broadcast"] = device.broadcast;
        obj["secureOn"] = device.secureOn;
        obj["category"] = device.category;
        obj["notes"] = device.notes;
        obj["created"] = device.created;
        obj["updated"] = device.updated;
    }


    void handleScanApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        constexpr size_t MAX_RESULTS = 254;

        /*
         * Static storage avoids placing the full scan result buffer on the
         * ESP32 task stack.
         */
        static DiscoveredHost hosts[MAX_RESULTS];

        IPAddress defaultStart;
        IPAddress defaultEnd;

        if (
            !NetworkScanner.getLocalHostRange(
                defaultStart,
                defaultEnd
            )
        )
        {
            sendJsonError(
                503,
                "Wi-Fi is not connected"
            );
            return;
        }

        IPAddress startIP =
            defaultStart;

        IPAddress endIP =
            defaultEnd;

        const bool customStart =
            server.hasArg("start") &&
            server.arg("start").length() > 0;

        const bool customEnd =
            server.hasArg("end") &&
            server.arg("end").length() > 0;

        if (
            customStart &&
            !startIP.fromString(
                server.arg("start")
            )
        )
        {
            sendJsonError(
                400,
                "Invalid start IP address"
            );
            return;
        }

        if (
            customEnd &&
            !endIP.fromString(
                server.arg("end")
            )
        )
        {
            sendJsonError(
                400,
                "Invalid end IP address"
            );
            return;
        }

        /*
         * Advanced Scan semantics:
         * - start + end -> scan exactly that range
         * - start only -> scan from start to the last usable address
         * - end only   -> scan from the first usable address to end
         * - neither    -> normal whole local host range
         */
        String validationError;

        if (
            !NetworkScanner.validateRange(
                startIP,
                endIP,
                validationError
            )
        )
        {
            sendJsonError(
                400,
                validationError.c_str()
            );
            return;
        }

        Logger.info(
            "NetworkScanner",
            "scan started; range=" +
            startIP.toString() +
            "-" +
            endIP.toString()
        );

        const size_t count =
            NetworkScanner.scanRange(
                startIP,
                endIP,
                hosts,
                MAX_RESULTS
            );

        Logger.info(
            "NetworkScanner",
            "scan completed; range=" +
            startIP.toString() +
            "-" +
            endIP.toString() +
            "; found=" +
            String(count)
        );

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        for (size_t i = 0; i < count; i++)
        {
            JsonObject obj =
                array.add<JsonObject>();

            obj["ip"] =
                hosts[i].ip.toString();

            obj["mac"] =
                hosts[i].mac;
        }

        String json;
        serializeJson(doc, json);

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleRoot()
    {
        File file =
            LittleFS.open(
                "/index.html",
                "r"
            );

        if (!file)
        {
            server.send(
                500,
                "text/plain",
                "index.html not found"
            );

            return;
        }

        server.streamFile(
            file,
            "text/html"
        );

        file.close();
    }


    void handleDevicesApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        const Device* devices =
            DeviceStore.getDevices();

        const size_t count =
            DeviceStore.getDeviceCount();

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        for (size_t i = 0; i < count; i++)
        {
            JsonObject obj = array.add<JsonObject>();
            addDeviceToJson(obj, devices[i]);
        }

        String json;
        serializeJson(doc, json);

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleAddDeviceApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        Device device{};

        if (!fillDeviceFromRequest(device, false))
        {
            return;
        }

        if (DeviceStore.macExists(device.mac))
        {
            Logger.warn(
                "DeviceStore",
                "add rejected; duplicate MAC=" +
                device.mac
            );

            sendJsonError(
                409,
                "A device with this MAC address already exists"
            );

            return;
        }

        if (!DeviceStore.add(device))
        {
            sendJsonError(
                500,
                "Unable to save device"
            );

            return;
        }

        Logger.info(
            "DeviceStore",
            "device added: " +
            device.name +
            " [" + device.mac + "]"
        );

        sendJsonSuccess(201);
    }


    void handleUpdateDeviceApi(uint32_t id)
    {
        const Device* existing =
            DeviceStore.getById(id);

        if (existing == nullptr)
        {
            sendJsonError(
                404,
                "Device not found"
            );

            return;
        }

        Device device = *existing;

        if (!fillDeviceFromRequest(device, true))
        {
            return;
        }

        if (DeviceStore.macExists(device.mac, id))
        {
            Logger.warn(
                "DeviceStore",
                "update rejected; duplicate MAC=" +
                device.mac
            );

            sendJsonError(
                409,
                "A device with this MAC address already exists"
            );

            return;
        }

        if (!DeviceStore.update(id, device))
        {
            sendJsonError(
                500,
                "Unable to update device"
            );

            return;
        }

        Logger.info(
            "DeviceStore",
            "device updated: " +
            device.name +
            " [" + String(id) + "]"
        );

        sendJsonSuccess();
    }


    void handleDeleteDeviceApi(uint32_t id)
    {
        const Device* existing =
            DeviceStore.getById(id);

        if (existing == nullptr)
        {
            sendJsonError(
                404,
                "Device not found"
            );

            return;
        }

        const String deletedName = existing->name;

        if (!DeviceStore.remove(id))
        {
            sendJsonError(
                500,
                "Unable to delete device"
            );

            return;
        }

        Logger.info(
            "DeviceStore",
            "device deleted: " +
            deletedName +
            " [" + String(id) + "]"
        );

        sendJsonSuccess();
    }




    void handleWakeDeviceApi(uint32_t id)
    {
        const Device* device =
            DeviceStore.getById(id);

        if (device == nullptr)
        {
            sendJsonError(
                404,
                "Device not found"
            );

            return;
        }

        Logger.info(
            "WebApp",
            "manual wake requested: " +
            device->name +
            " [" + String(id) + "]"
        );

        if (!WakeEngine.wakeManual(*device))
        {
            sendJsonError(
                500,
                "Unable to start wake sequence"
            );

            return;
        }

        sendJsonSuccess(202);
    }


    void handleWakeAllDevicesApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        const Device* devices =
            DeviceStore.getDevices();

        const size_t count =
            DeviceStore.getDeviceCount();

        size_t eligible = 0;
        size_t queued = 0;
        size_t failed = 0;

        Logger.info(
            "WakeAll",
            "manual Wake All requested for " +
            String(count) +
            " configured device(s)"
        );

        for (size_t i = 0; i < count; ++i)
        {
            const Device& device =
                devices[i];

            if (!device.enabled)
            {
                Logger.info(
                    "WakeAll",
                    "skip " +
                    device.name +
                    " [" +
                    String(device.id) +
                    "] - disabled"
                );
                continue;
            }

            eligible++;

            Logger.info(
                "WakeAll",
                "queue " +
                device.name +
                " [" +
                String(device.id) +
                "]"
            );

            if (WakeEngine.wakeManual(device))
            {
                queued++;
            }
            else
            {
                failed++;

                Logger.warn(
                    "WakeAll",
                    "unable to queue " +
                    device.name +
                    " [" +
                    String(device.id) +
                    "]"
                );
            }
        }

        Logger.info(
            "WakeAll",
            "completed: eligible=" +
            String(eligible) +
            " queued=" +
            String(queued) +
            " failed=" +
            String(failed)
        );

        JsonDocument doc;
        doc["success"] = failed == 0;
        doc["configured"] = count;
        doc["eligible"] = eligible;
        doc["queued"] = queued;
        doc["failed"] = failed;

        String json;
        serializeJson(doc, json);

        server.send(
            failed == 0 ? 202 : 207,
            "application/json",
            json
        );
    }


    void handleExportDevicesConfigApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        Logger.info(
            "ConfigExport",
            "device configuration export requested"
        );

        String json;

        if (!DeviceStore.exportConfiguration(json))
        {
            sendJsonError(
                500,
                "Unable to export configuration"
            );

            return;
        }

        server.sendHeader(
            "Content-Disposition",
            "attachment; filename=\"saved_devices.json\""
        );

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleImportDevicesConfigApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        if (!server.hasArg("plain"))
        {
            sendJsonError(
                400,
                "Missing configuration body"
            );

            return;
        }

        String errorMessage;

        if (!DeviceStore.importConfiguration(
                server.arg("plain"),
                errorMessage))
        {
            Logger.warn(
                "ConfigImport",
                "restore rejected: " + errorMessage
            );
            sendJsonError(
                400,
                errorMessage.c_str()
            );

            return;
        }

        Logger.info(
            "ConfigImport",
            "restore completed; devices=" +
            String(DeviceStore.getDeviceCount())
        );

        sendJsonSuccess();
    }


    void handleLogFilesApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        JsonDocument doc;

        doc["retentionDays"] =
            Config.getLogRetentionDays();

        doc["maxFileBytes"] =
            Logger.getMaxFileBytes();

        doc["current"] =
            Logger.getCurrentFileName();

        doc["persistedLines"] =
            Logger.getPersistedLineCount();

        doc["writeFailures"] =
            Logger.getWriteFailureCount();

        JsonArray files =
            doc["files"].to<JsonArray>();

        File directory =
            LittleFS.open("/logs");

        if (directory && directory.isDirectory())
        {
            File file =
                directory.openNextFile();

            while (file)
            {
                String fullName = file.name();
                String fileName = fullName;

                const int slash =
                    fileName.lastIndexOf('/');

                if (slash >= 0)
                {
                    fileName =
                        fileName.substring(slash + 1);
                }

                if (Logger.isValidLogFileName(fileName))
                {
                    JsonObject item =
                        files.add<JsonObject>();

                    item["name"] = fileName;
                    item["size"] = file.size();
                    item["current"] =
                        fileName ==
                        Logger.getCurrentFileName();
                }

                file.close();
                file =
                    directory.openNextFile();
            }

            directory.close();
        }

        String json;
        serializeJson(doc, json);

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleLogFileApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        String fileName =
            server.arg("name");

        if (fileName.length() == 0)
        {
            fileName =
                Logger.getCurrentFileName();
        }

        if (!Logger.isValidLogFileName(fileName))
        {
            sendJsonError(
                400,
                "Invalid log file name"
            );
            return;
        }

        const String path =
            "/logs/" + fileName;

        if (!LittleFS.exists(path))
        {
            sendJsonError(
                404,
                "Log file not found"
            );
            return;
        }

        File file =
            LittleFS.open(path, "r");

        if (!file)
        {
            sendJsonError(
                500,
                "Unable to open log file"
            );
            return;
        }

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.streamFile(
            file,
            "text/plain"
        );

        file.close();
    }



    bool isValidLanguageCode(
        const String& code
    )
    {
        if (
            code.length() == 0 ||
            code.length() > 16
        )
        {
            return false;
        }

        for (size_t i = 0; i < code.length(); ++i)
        {
            const char c = code[i];

            if (
                !isAlphaNumeric(c) &&
                c != '-' &&
                c != '_'
            )
            {
                return false;
            }
        }

        return true;
    }


    String languageFilePath(
        const String& code
    )
    {
        return
            "/lang/" +
            code +
            ".properties";
    }


    bool isLanguageAvailable(
        const String& code
    )
    {
        return
            isValidLanguageCode(code) &&
            LittleFS.exists(
                languageFilePath(code)
            );
    }


    String readLanguageProperty(
        File& file,
        const String& wantedKey
    )
    {
        file.seek(0);

        while (file.available())
        {
            String line =
                file.readStringUntil('\n');

            line.trim();

            if (
                line.length() == 0 ||
                line.startsWith("#") ||
                line.startsWith(";")
            )
            {
                continue;
            }

            const int separator =
                line.indexOf('=');

            if (separator <= 0)
            {
                continue;
            }

            String key =
                line.substring(
                    0,
                    separator
                );

            key.trim();

            if (key != wantedKey)
            {
                continue;
            }

            String value =
                line.substring(
                    separator + 1
                );

            value.trim();

            return value;
        }

        return "";
    }


    void handleLanguagesApi()
    {
        JsonDocument doc;

        String current =
            Config.getLanguage();

        if (!isLanguageAvailable(current))
        {
            current = "en";
        }

        doc["current"] =
            current;

        JsonArray languages =
            doc["languages"].to<JsonArray>();

        File directory =
            LittleFS.open("/lang");

        if (
            directory &&
            directory.isDirectory()
        )
        {
            File file =
                directory.openNextFile();

            while (file)
            {
                if (!file.isDirectory())
                {
                    String name =
                        file.name();

                    const int slash =
                        name.lastIndexOf('/');

                    if (slash >= 0)
                    {
                        name =
                            name.substring(
                                slash + 1
                            );
                    }

                    if (
                        name.endsWith(
                            ".properties"
                        )
                    )
                    {
                        const String codeFromFile =
                            name.substring(
                                0,
                                name.length() -
                                String(".properties").length()
                            );

                        if (
                            isValidLanguageCode(
                                codeFromFile
                            )
                        )
                        {
                            String code =
                                readLanguageProperty(
                                    file,
                                    "language.code"
                                );

                            String displayName =
                                readLanguageProperty(
                                    file,
                                    "language.name"
                                );

                            if (
                                !isValidLanguageCode(
                                    code
                                )
                            )
                            {
                                code =
                                    codeFromFile;
                            }

                            /*
                             * Filename is authoritative for loading.
                             * Metadata code is descriptive and must match it.
                             */
                            if (code != codeFromFile)
                            {
                                code =
                                    codeFromFile;
                            }

                            if (
                                displayName.length() == 0
                            )
                            {
                                displayName =
                                    code;
                            }

                            JsonObject item =
                                languages.add<JsonObject>();

                            item["code"] =
                                code;

                            item["name"] =
                                displayName;

                            item["file"] =
                                name;
                        }
                    }
                }

                file.close();

                file =
                    directory.openNextFile();
            }

            directory.close();
        }

        String json;
        serializeJson(doc, json);

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleLanguageSaveApi()
    {
        /*
         * A brand-new WakeWizard must allow language selection before the
         * administrator password exists. Once provisioned, changing the
         * persistent language requires a valid authenticated session.
         */
        if (
            Config.isProvisioned() &&
            !ensureAuthenticated()
        )
        {
            return;
        }

        JsonDocument doc;

        if (
            deserializeJson(
                doc,
                server.arg("plain")
            )
        )
        {
            sendJsonError(
                400,
                "Invalid language request"
            );
            return;
        }

        String language =
            doc["language"] |
            "";

        language.trim();

        if (
            !isLanguageAvailable(
                language
            )
        )
        {
            sendJsonError(
                400,
                "Language is not available"
            );
            return;
        }

        Config.setLanguage(
            language
        );

        if (!Config.save())
        {
            sendJsonError(
                500,
                "Unable to save language"
            );
            return;
        }

        Logger.info(
            "Config",
            "language changed to " +
            language
        );

        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["language"] = language;

        String json;
        serializeJson(
            responseDoc,
            json
        );

        server.send(
            200,
            "application/json",
            json
        );
    }


    bool isValidWakeWizardHostname(
        const String& hostname
    )
    {
        if (
            hostname.length() == 0 ||
            hostname.length() > 32
        )
        {
            return false;
        }

        if (
            hostname[0] == '-' ||
            hostname[
                hostname.length() - 1
            ] == '-'
        )
        {
            return false;
        }

        for (
            size_t i = 0;
            i < hostname.length();
            i++
        )
        {
            const char c = hostname[i];

            if (
                !isalnum(
                    static_cast<unsigned char>(c)
                ) &&
                c != '-'
            )
            {
                return false;
            }
        }

        return true;
    }


    void handleWakeWizardConfigGetApi()
    {
        if (!ensureSetupOrAuthenticated())
        {
            return;
        }

        String json;

        if (!Config.toPublicJson(json))
        {
            sendJsonError(
                500,
                "Unable to serialize WakeWizard configuration"
            );
            return;
        }

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.send(
            200,
            "application/json",
            json
        );
    }


    void handleWakeWizardConfigSaveApi()
    {
        if (!ensureSetupOrAuthenticated())
        {
            return;
        }

        JsonDocument doc;

        if (
            deserializeJson(
                doc,
                server.arg("plain")
            )
        )
        {
            sendJsonError(
                400,
                "Invalid JSON"
            );
            return;
        }

        const bool firstSetup =
            !Config.isProvisioned();

        String hostname =
            doc["hostname"] | "";

        hostname.trim();
        hostname.toLowerCase();

        const String ssid =
            doc["ssid"] | "";

        const String wifiPassword =
            doc["wifiPassword"] | "";

        const bool wifiOpen =
            doc["wifiOpen"] | false;

        const String adminPassword =
            doc["adminPassword"] | "";

        const String adminPasswordConfirm =
            doc["adminPasswordConfirm"] | "";

        const int retention =
            doc["logRetentionDays"] | 8;

        String language =
            doc["language"] |
            Config.getLanguage();

        language.trim();

        if (!isLanguageAvailable(language))
        {
            sendJsonError(
                400,
                "Selected language is not available"
            );
            return;
        }

        if (
            !isValidWakeWizardHostname(
                hostname
            ) ||
            ssid.length() == 0 ||
            retention < 1 ||
            retention > 30
        )
        {
            sendJsonError(
                400,
                "Invalid configuration. Hostname may contain only letters, numbers and hyphens."
            );
            return;
        }

        if (
            adminPassword !=
            adminPasswordConfirm
        )
        {
            sendJsonError(
                400,
                "Administrator passwords do not match"
            );
            return;
        }

        if (
            firstSetup &&
            adminPassword.length() < 8
        )
        {
            sendJsonError(
                400,
                "An admin password of at least 8 characters is required during initial setup"
            );
            return;
        }

        const bool ssidChanged =
            ssid != Config.getWifiSSID();

        if (
            ssidChanged &&
            wifiPassword.length() == 0 &&
            !wifiOpen)
        {
            sendJsonError(
                400,
                "A Wi-Fi password is required when changing to a protected network"
            );
            return;
        }

        Config.setHostname(
            hostname
        );

        Config.setWifiSSID(
            ssid
        );

        /*
         * An empty password preserves the current password only while the
         * SSID remains unchanged. Changing to an open network explicitly
         * clears any password saved for the previous SSID.
         */
        if (
            firstSetup ||
            ssidChanged ||
            wifiPassword.length() > 0
        )
        {
            Config.setWifiPassword(
                wifiPassword
            );
        }

        Config.setLanguage(
            language
        );

        Config.setLogRetentionDays(
            retention
        );

        if (
            adminPassword.length() > 0 &&
            !Config.setAdminPassword(
                adminPassword
            )
        )
        {
            sendJsonError(
                400,
                "Admin password must be at least 8 characters"
            );
            return;
        }

        if (!Config.save())
        {
            sendJsonError(
                500,
                "Unable to save configuration"
            );
            return;
        }

        /*
         * This is the condition that matters. Do not reboot from a
         * configuration that still considers itself unprovisioned.
         */
        if (!Config.isProvisioned())
        {
            sendJsonError(
                500,
                "Configuration was saved but is incomplete"
            );
            return;
        }

        Logger.info(
            "Config",
            "WakeWizard configuration saved; provisioned=true; hostname=" +
            String(Config.getHostname()) +
            "; SSID=" +
            String(Config.getWifiSSID())
        );

        sendJsonSuccess();

        /*
         * Any network/hostname change is safest when applied by reboot.
         * This also makes first-setup behavior deterministic.
         */
        restartRequestedAt =
            millis() + 1500;
    }


    void handleWakeWizardConfigExportApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        String json;

        if (!Config.exportConfiguration(json))
        {
            sendJsonError(
                500,
                "Unable to export WakeWizard configuration"
            );
            return;
        }

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.sendHeader(
            "Content-Disposition",
            "attachment; filename=\"wakewizard_config.json\""
        );

        server.send(
            200,
            "application/json",
            json
        );
    }




    void handleWifiNetworksApi()
    {
        if (!ensureSetupOrAuthenticated())
        {
            return;
        }

        const int count = WiFi.scanNetworks(false, true);
        JsonDocument doc;
        JsonArray networks = doc["networks"].to<JsonArray>();

        for (int i = 0; i < count; ++i)
        {
            const String ssid = WiFi.SSID(i);
            bool duplicate = false;
            for (JsonObject item : networks)
            {
                if (String(item["ssid"] | "") == ssid) { duplicate = true; break; }
            }
            if (duplicate || ssid.length() == 0) continue;

            JsonObject item = networks.add<JsonObject>();
            item["ssid"] = ssid;
            item["rssi"] = WiFi.RSSI(i);
            item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }

        WiFi.scanDelete();
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    }

    void handleSystemApi()
    {
        if (!ensureSetupOrAuthenticated())
        {
            return;
        }

        JsonDocument doc;

        doc["hostname"] = Config.getHostname();
        doc["ip"] = Network.getIPAddress();
        doc["mac"] = Network.getMACAddress();
        doc["setupMode"] = Network.isSetupMode();
        doc["setupSsid"] = Network.getSetupSSID();
        doc["wifiConnected"] = Network.isConnected();
        doc["ssid"] = Network.getSSID();
        doc["rssi"] = Network.getRSSI();
        doc["version"] = Config.getFirmwareVersion();
        doc["logRetentionDays"] = Config.getLogRetentionDays();

        doc["uptimeSeconds"] = millis() / 1000UL;
        doc["chipModel"] = ESP.getChipModel();
        doc["chipRevision"] = ESP.getChipRevision();
        doc["cpuMHz"] = ESP.getCpuFreqMHz();
        doc["flashBytes"] = ESP.getFlashChipSize();
        doc["freeHeapBytes"] = ESP.getFreeHeap();
        doc["minFreeHeapBytes"] = ESP.getMinFreeHeap();

        doc["subnetMask"] = WiFi.subnetMask().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();

        doc["fsTotalBytes"] = LittleFS.totalBytes();
        doc["fsUsedBytes"] = LittleFS.usedBytes();
        doc["fsFreeBytes"] =
            LittleFS.totalBytes() >= LittleFS.usedBytes()
                ? LittleFS.totalBytes() - LittleFS.usedBytes()
                : 0;

        doc["timeSyncImplemented"] = true;
        doc["timeSyncRequested"] =
            Logger.isTimeSyncRequested();

        doc["timeSynchronized"] =
            Logger.isTimeSynchronized();

        doc["currentUtcTime"] =
            Logger.getCurrentUtcTime();

        doc["timeZone"] = "UTC";

        doc["ntpServers"] =
            Logger.getNtpServers();

        String json;
        serializeJson(doc, json);

        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", json);
    }


    void handleSystemRebootApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        Logger.warn("System", "reboot requested from web UI");
        sendJsonSuccess();
        restartRequestedAt = millis() + 1000;
    }


    void handleSystemFactoryResetApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        Logger.warn(
            "System",
            "factory reset requested from web UI"
        );

        if (!DeviceStore.clear())
        {
            sendJsonError(
                500,
                "Unable to erase saved devices"
            );
            return;
        }

        if (!Config.factoryReset())
        {
            sendJsonError(
                500,
                "Unable to erase WakeWizard configuration"
            );
            return;
        }

        Logger.warn(
            "System",
            "factory reset completed; configuration and saved devices erased"
        );

        sendJsonSuccess();
        restartRequestedAt = millis() + 1000;
    }


    void handleOtaUploadComplete()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        if (Update.hasError())
        {
            sendJsonError(500, "OTA update failed");
            return;
        }

        sendJsonSuccess();
        restartRequestedAt = millis() + 1200;
    }


    void handleFirmwareUpload()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        HTTPUpload& upload = server.upload();

        if (upload.status == UPLOAD_FILE_START)
        {
            Logger.info("OTA", "firmware upload started");

            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (!Update.end(true))
            {
                Update.printError(Serial);
            }
            else
            {
                Logger.info("OTA", "firmware upload completed");
            }
        }
        else if (upload.status == UPLOAD_FILE_ABORTED)
        {
            Update.abort();
            Logger.warn("OTA", "firmware upload aborted");
        }
    }


    void handleFilesystemUpload()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        HTTPUpload& upload = server.upload();

        if (upload.status == UPLOAD_FILE_START)
        {
            Logger.info(
                "OTA",
                "filesystem upload started"
            );

            LittleFS.end();

            if (
                !Update.begin(
                    UPDATE_SIZE_UNKNOWN,
                    U_SPIFFS
                )
            )
            {
                Update.printError(Serial);

                /*
                 * Update never started, so restore access to the existing
                 * filesystem immediately.
                 */
                if (!LittleFS.begin(false))
                {
                    Serial.println(
                        "[OTA] LittleFS remount failed after update start failure"
                    );
                }
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (
                Update.write(
                    upload.buf,
                    upload.currentSize
                ) != upload.currentSize
            )
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (!Update.end(true))
            {
                Update.printError(Serial);

                /*
                 * A failed filesystem OTA must not leave the web UI/logging
                 * without a mounted LittleFS until a manual reboot.
                 */
                if (!LittleFS.begin(false))
                {
                    Serial.println(
                        "[OTA] LittleFS remount failed after upload failure"
                    );
                }

                Logger.error(
                    "OTA",
                    "filesystem upload failed"
                );
            }
            else
            {
                /*
                 * Do not remount the newly written filesystem here. The
                 * completion handler schedules the normal reboot.
                 */
                Serial.println(
                    "[OTA] filesystem upload completed"
                );
            }
        }
        else if (
            upload.status ==
            UPLOAD_FILE_ABORTED
        )
        {
            Update.abort();

            if (!LittleFS.begin(false))
            {
                Serial.println(
                    "[OTA] LittleFS remount failed after upload abort"
                );
            }

            Logger.warn(
                "OTA",
                "filesystem upload aborted"
            );
        }
    }


    void handleNotFound()
    {
        if (
            server.uri().startsWith("/api/") &&
            !ensureAuthenticated()
        )
        {
            return;
        }

        uint32_t id = 0;

        if (
            server.method() == HTTP_POST &&
            extractWakeDeviceId(
                server.uri(),
                id
            )
        )
        {
            handleWakeDeviceApi(id);
            return;
        }

        if (extractDeviceId(server.uri(), id))
        {
            if (server.method() == HTTP_PUT)
            {
                handleUpdateDeviceApi(id);
                return;
            }

            if (server.method() == HTTP_DELETE)
            {
                handleDeleteDeviceApi(id);
                return;
            }
        }

        Logger.warn(
            "WebApp",
            "request not found: " +
            server.uri()
        );

        server.send(
            404,
            "application/json",
            "{\"success\":false,\"message\":\"Not found\"}"
        );
    }


    void handleDownloadLogFileApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        String fileName = server.arg("name");

        if (fileName.length() == 0)
        {
            fileName = Logger.getCurrentFileName();
        }

        if (!Logger.isValidLogFileName(fileName))
        {
            sendJsonError(400, "Invalid log file name");
            return;
        }

        const String path = "/logs/" + fileName;

        if (!LittleFS.exists(path))
        {
            sendJsonError(404, "Log file not found");
            return;
        }

        File file = LittleFS.open(path, "r");

        if (!file)
        {
            sendJsonError(500, "Unable to open log file");
            return;
        }

        server.sendHeader("Cache-Control", "no-store");
        server.sendHeader(
            "Content-Disposition",
            "attachment; filename=\"" + fileName + "\""
        );

        server.streamFile(file, "text/plain");
        file.close();
    }


    void handleDeleteLogFileApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        const String fileName = server.arg("name");

        if (fileName.length() == 0)
        {
            sendJsonError(400, "Missing log file name");
            return;
        }

        String errorMessage;

        if (!Logger.deleteLogFile(fileName, errorMessage))
        {
            sendJsonError(
                errorMessage == "Log file not found" ? 404 : 400,
                errorMessage.c_str()
            );
            return;
        }

        sendJsonSuccess();
    }


    void handleDeleteLogHistoryApi()
    {
        if (!ensureAuthenticated())
        {
            return;
        }

        const size_t deleted = Logger.deleteHistory();

        JsonDocument doc;
        doc["success"] = true;
        doc["deleted"] = deleted;

        String json;
        serializeJson(doc, json);

        server.send(200, "application/json", json);
    }

}


WebAppClass WebApp;


bool WebAppClass::begin()
{
    // LittleFS is initialized centrally in main.cpp before Logger and
    // the rest of the application services are started.

    // Main page
    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    // Authentication API
    server.on(
        "/api/auth/status",
        HTTP_GET,
        handleAuthStatusApi
    );

    server.on(
        "/api/auth/login",
        HTTP_POST,
        handleAuthLoginApi
    );

    server.on(
        "/api/auth/logout",
        HTTP_POST,
        handleAuthLogoutApi
    );

    // Language discovery is public so the Initial Setup page can be localized.
    server.on(
        "/api/languages",
        HTTP_GET,
        handleLanguagesApi
    );

    server.on(
        "/api/config/language",
        HTTP_POST,
        handleLanguageSaveApi
    );

    // REST API
    server.on(
        "/api/system",
        HTTP_GET,
        handleSystemApi
    );

    server.on(
        "/api/system/reboot",
        HTTP_POST,
        handleSystemRebootApi
    );

    server.on(
        "/api/system/factory-reset",
        HTTP_POST,
        handleSystemFactoryResetApi
    );

    server.on(
        "/api/system/ota/firmware",
        HTTP_POST,
        handleOtaUploadComplete,
        handleFirmwareUpload
    );

    server.on(
        "/api/system/ota/filesystem",
        HTTP_POST,
        handleOtaUploadComplete,
        handleFilesystemUpload
    );

    server.on(
        "/api/config/wakewizard",
        HTTP_GET,
        handleWakeWizardConfigGetApi
    );

    server.on(
        "/api/config/wakewizard",
        HTTP_POST,
        handleWakeWizardConfigSaveApi
    );


    server.on(
        "/api/config/wakewizard/export",
        HTTP_GET,
        handleWakeWizardConfigExportApi
    );

    server.on(
        "/api/wifi/networks",
        HTTP_GET,
        handleWifiNetworksApi
    );

    server.on(
        "/api/devices",
        HTTP_GET,
        handleDevicesApi
    );

    server.on(
        "/api/devices",
        HTTP_POST,
        handleAddDeviceApi
    );

    server.on(
        "/api/devices/wake-all",
        HTTP_POST,
        handleWakeAllDevicesApi
    );

    server.on(
        "/api/scan",
        HTTP_GET,
        handleScanApi
    );

    server.on(
        "/api/config/devices/export",
        HTTP_GET,
        handleExportDevicesConfigApi
    );

    server.on(
        "/api/config/devices/import",
        HTTP_POST,
        handleImportDevicesConfigApi
    );

    server.on(
        "/api/logs/files",
        HTTP_GET,
        handleLogFilesApi
    );

    server.on(
        "/api/logs/file",
        HTTP_GET,
        handleLogFileApi
    );

    server.on(
        "/api/logs/download",
        HTTP_GET,
        handleDownloadLogFileApi
    );

    server.on(
        "/api/logs/file",
        HTTP_DELETE,
        handleDeleteLogFileApi
    );

    server.on(
        "/api/logs/history",
        HTTP_DELETE,
        handleDeleteLogHistoryApi
    );

    // Static Web UI resources
    server.serveStatic(
        "/css/",
        LittleFS,
        "/css/"
    );

    server.serveStatic(
        "/js/",
        LittleFS,
        "/js/"
    );

    server.serveStatic(
        "/img/",
        LittleFS,
        "/img/"
    );

    server.serveStatic(
        "/lang/",
        LittleFS,
        "/lang/"
    );

    server.onNotFound(handleNotFound);

    const char* headerKeys[] = {
        "Cookie"
    };

    server.collectHeaders(
        headerKeys,
        1
    );

    server.begin();

    return true;
}


void WebAppClass::loop()
{
    server.handleClient();

    if (
        restartRequestedAt != 0 &&
        static_cast<int32_t>(
            millis() - restartRequestedAt
        ) >= 0
    )
    {
        restartRequestedAt = 0;

        Logger.info(
            "System",
            "restarting after configuration"
        );

        delay(100);
        ESP.restart();
    }
}
