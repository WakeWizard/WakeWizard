#include "Config.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <nvs.h>

constexpr uint32_t ConfigClass::FILE_VERSION;
constexpr uint8_t ConfigClass::DEFAULT_LOG_RETENTION_DAYS;

namespace
{
    constexpr const char* NVS_NAMESPACE = "wakewizard";
    constexpr const char* LEGACY_CONFIG_PATH = "/wakewizard_config.json";
    constexpr const char* FIRMWARE_VERSION = "1.0.0";

    String toHex(
        const unsigned char* data,
        size_t length
    )
    {
        static const char* HEX_CHARS =
            "0123456789abcdef";

        String result;
        result.reserve(length * 2);

        for (size_t i = 0; i < length; ++i)
        {
            result +=
                HEX_CHARS[
                    (data[i] >> 4) & 0x0F
                ];

            result +=
                HEX_CHARS[
                    data[i] & 0x0F
                ];
        }

        return result;
    }
}

ConfigClass Config;


void ConfigClass::applyDefaults()
{
    wifiSSID = "";
    wifiPassword = "";
    hostname = "wakewizard";
    language = DEFAULT_LANGUAGE;
    adminSalt = "";
    adminPasswordHash = "";
    logRetentionDays =
        DEFAULT_LOG_RETENTION_DAYS;
    provisioned = false;
}


void ConfigClass::recomputeProvisioned()
{
    /*
     * Do not trust a stale persisted boolean.
     * A device is provisioned only when all data needed for a normal
     * boot is really present.
     */
    provisioned =
        wifiSSID.length() > 0 &&
        hostname.length() > 0 &&
        adminSalt.length() > 0 &&
        adminPasswordHash.length() > 0;
}


bool ConfigClass::begin()
{
    applyDefaults();

    if (!loadFromNvs())
    {
        return false;
    }

    /*
     * Upgrade path from Patch 007/007a/007b:
     * if NVS has no usable configuration but the old LittleFS file
     * exists, migrate it once to NVS.
     */
    if (
        !provisioned &&
        LittleFS.exists(
            LEGACY_CONFIG_PATH
        )
    )
    {
        if (!migrateLegacyLittleFsConfig())
        {
            return false;
        }
    }

    recomputeProvisioned();

    return true;
}


bool ConfigClass::loadFromNvs()
{
    Preferences prefs;

    /*
     * Do NOT open Preferences read-only here.
     *
     * On a brand-new ESP32 the "wakewizard" namespace does not exist yet.
     * Preferences.begin(..., true) therefore fails with:
     *
     *     nvs_open failed: NOT_FOUND
     *
     * Opening read/write creates the namespace when necessary. We still
     * only read values in this function.
     */
    if (!prefs.begin(
            NVS_NAMESPACE,
            false
        ))
    {
        return false;
    }

    hostname =
        prefs.getString(
            "hostname",
            "wakewizard"
        );

    language =
        prefs.getString(
            "lang",
            DEFAULT_LANGUAGE
        );

    if (language.length() == 0)
    {
        language = DEFAULT_LANGUAGE;
    }

    wifiSSID =
        prefs.getString(
            "ssid",
            ""
        );

    wifiPassword =
        prefs.getString(
            "wifiPwd",
            ""
        );

    adminSalt =
        prefs.getString(
            "adminSalt",
            ""
        );

    adminPasswordHash =
        prefs.getString(
            "adminHash",
            ""
        );

    logRetentionDays =
        constrain(
            static_cast<int>(
                prefs.getUChar(
                    "logRet",
                    DEFAULT_LOG_RETENTION_DAYS
                )
            ),
            1,
            30
        );

    prefs.end();

    recomputeProvisioned();

    return true;
}


bool ConfigClass::migrateLegacyLittleFsConfig()
{
    File file =
        LittleFS.open(
            LEGACY_CONFIG_PATH,
            "r"
        );

    if (!file)
    {
        return false;
    }

    JsonDocument doc;

    const DeserializationError error =
        deserializeJson(
            doc,
            file
        );

    file.close();

    if (error)
    {
        return false;
    }

    hostname =
        doc["hostname"] |
        "wakewizard";

    logRetentionDays =
        constrain(
            static_cast<int>(
                doc["logRetentionDays"] |
                DEFAULT_LOG_RETENTION_DAYS
            ),
            1,
            30
        );

    language =
        doc["language"] |
        DEFAULT_LANGUAGE;

    if (language.length() == 0)
    {
        language = DEFAULT_LANGUAGE;
    }

    wifiSSID =
        doc["wifi"]["ssid"] |
        "";

    wifiPassword =
        doc["wifi"]["password"] |
        "";

    adminSalt =
        doc["security"]["salt"] |
        "";

    adminPasswordHash =
        doc["security"]["passwordHash"] |
        "";

    recomputeProvisioned();

    if (!save())
    {
        return false;
    }

    /*
     * Legacy file contains secrets. Once migrated successfully, remove it.
     */
    LittleFS.remove(
        LEGACY_CONFIG_PATH
    );

    return true;
}


bool ConfigClass::save()
{
    recomputeProvisioned();

    nvs_handle_t handle;

    if (nvs_open(
            NVS_NAMESPACE,
            NVS_READWRITE,
            &handle
        ) != ESP_OK)
    {
        return false;
    }

    esp_err_t error =
        nvs_set_u32(
            handle,
            "version",
            FILE_VERSION
        );

    if (error == ESP_OK)
    {
        error = nvs_set_str(handle, "hostname", hostname.c_str());
    }
    if (error == ESP_OK)
    {
        error = nvs_set_str(handle, "lang", language.c_str());
    }
    if (error == ESP_OK)
    {
        error = nvs_set_str(handle, "ssid", wifiSSID.c_str());
    }
    if (error == ESP_OK)
    {
        error = nvs_set_str(handle, "wifiPwd", wifiPassword.c_str());
    }
    if (error == ESP_OK)
    {
        error = nvs_set_u8(handle, "logRet", logRetentionDays);
    }
    if (error == ESP_OK)
    {
        error = nvs_set_str(handle, "adminSalt", adminSalt.c_str());
    }
    if (error == ESP_OK)
    {
        error = nvs_set_str(handle, "adminHash", adminPasswordHash.c_str());
    }
    if (error == ESP_OK)
    {
        error = nvs_set_u8(handle, "provisioned", provisioned ? 1 : 0);
    }

    if (error == ESP_OK)
    {
        error = nvs_commit(handle);
    }

    nvs_close(handle);

    return error == ESP_OK;
}


const char* ConfigClass::getWifiSSID() const
{
    return wifiSSID.c_str();
}


const char* ConfigClass::getWifiPassword() const
{
    return wifiPassword.c_str();
}


const char* ConfigClass::getHostname() const
{
    return hostname.c_str();
}


const char* ConfigClass::getFirmwareVersion() const
{
    return FIRMWARE_VERSION;
}


const char* ConfigClass::getLanguage() const
{
    return language.c_str();
}


uint8_t ConfigClass::getLogRetentionDays() const
{
    return logRetentionDays;
}


bool ConfigClass::isProvisioned() const
{
    return provisioned;
}


void ConfigClass::setWifiSSID(
    const String& value
)
{
    wifiSSID = value;
    recomputeProvisioned();
}


void ConfigClass::setWifiPassword(
    const String& value
)
{
    wifiPassword = value;
}


void ConfigClass::setHostname(
    const String& value
)
{
    hostname = value;
    recomputeProvisioned();
}


void ConfigClass::setLanguage(
    const String& value
)
{
    language = value;
    language.trim();

    if (language.length() == 0)
    {
        language = DEFAULT_LANGUAGE;
    }
}


void ConfigClass::setLogRetentionDays(
    uint8_t value
)
{
    logRetentionDays =
        constrain(
            value,
            1,
            30
        );
}


String ConfigClass::makeSalt()
{
    char buffer[33];

    for (int i = 0; i < 16; ++i)
    {
        sprintf(
            buffer + (i * 2),
            "%02x",
            static_cast<unsigned>(
                esp_random() & 0xFF
            )
        );
    }

    buffer[32] = '\0';

    return String(buffer);
}


String ConfigClass::hashPassword(
    const String& password,
    const String& salt
)
{
    const String material =
        salt +
        ":" +
        password;

    unsigned char digest[32];

    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    mbedtls_sha256_update(
        &ctx,
        reinterpret_cast<
            const unsigned char*
        >(material.c_str()),
        material.length()
    );

    mbedtls_sha256_finish(
        &ctx,
        digest
    );

    mbedtls_sha256_free(&ctx);

    return toHex(
        digest,
        sizeof(digest)
    );
}


bool ConfigClass::setAdminPassword(
    const String& password
)
{
    if (password.length() < 8)
    {
        return false;
    }

    adminSalt =
        makeSalt();

    adminPasswordHash =
        hashPassword(
            password,
            adminSalt
        );

    recomputeProvisioned();

    return true;
}


bool ConfigClass::verifyAdminPassword(
    const String& password
) const
{
    return
        adminSalt.length() > 0 &&
        adminPasswordHash ==
            hashPassword(
                password,
                adminSalt
            );
}


bool ConfigClass::toPublicJson(
    String& json
) const
{
    JsonDocument doc;

    doc["version"] = FILE_VERSION;
    doc["provisioned"] = provisioned;
    doc["hostname"] = hostname;
    doc["language"] = language;
    doc["logRetentionDays"] = logRetentionDays;
    doc["wifi"]["ssid"] = wifiSSID;
    doc["secretsIncluded"] = false;

    serializeJson(doc, json);
    return true;
}


bool ConfigClass::exportConfiguration(
    String& json
) const
{
    JsonDocument doc;

    doc["version"] =
        FILE_VERSION;

    doc["hostname"] =
        hostname;

    doc["language"] =
        language;

    doc["logRetentionDays"] =
        logRetentionDays;

    doc["wifi"]["ssid"] =
        wifiSSID;

    /*
     * Intentionally never export Wi-Fi/admin secrets.
     */
    doc["secretsIncluded"] =
        false;

    serializeJsonPretty(
        doc,
        json
    );

    return true;
}


bool ConfigClass::migrateImportedConfig(
    JsonDocument& doc,
    String& errorMessage
) const
{
    const uint32_t version =
        doc["version"] | 1;

    if (version > FILE_VERSION)
    {
        errorMessage =
            "Configuration was created by a newer WakeWizard version";

        return false;
    }

    if (
        !doc["hostname"].is<
            const char*
        >()
    )
    {
        doc["hostname"] =
            "wakewizard";
    }

    if (
        !doc["logRetentionDays"].is<
            int
        >()
    )
    {
        doc["logRetentionDays"] =
            DEFAULT_LOG_RETENTION_DAYS;
    }

    if (
        !doc["language"].is<
            const char*
        >()
    )
    {
        doc["language"] =
            DEFAULT_LANGUAGE;
    }

    doc["version"] =
        FILE_VERSION;

    return true;
}


bool ConfigClass::importConfiguration(
    const String& json,
    String& errorMessage
)
{
    JsonDocument doc;

    if (
        deserializeJson(
            doc,
            json
        )
    )
    {
        errorMessage =
            "Invalid WakeWizard configuration";

        return false;
    }

    if (
        !migrateImportedConfig(
            doc,
            errorMessage
        )
    )
    {
        return false;
    }

    const String newHostname =
        doc["hostname"] |
        "wakewizard";

    const int retention =
        doc["logRetentionDays"] |
        DEFAULT_LOG_RETENTION_DAYS;

    const String importedLanguage =
        doc["language"] |
        DEFAULT_LANGUAGE;

    if (
        newHostname.length() == 0 ||
        retention < 1 ||
        retention > 30
    )
    {
        errorMessage =
            "Invalid WakeWizard configuration values";

        return false;
    }

    hostname =
        newHostname;

    language =
        importedLanguage.length() > 0
            ? importedLanguage
            : DEFAULT_LANGUAGE;

    logRetentionDays =
        retention;

    const String importedSSID =
        doc["wifi"]["ssid"] |
        "";

    if (importedSSID.length() > 0)
    {
        wifiSSID =
            importedSSID;
    }

    /*
     * Import intentionally leaves existing secrets untouched.
     */
    recomputeProvisioned();

    if (!save())
    {
        errorMessage =
            "Unable to save WakeWizard configuration";

        return false;
    }

    errorMessage = "";

    return true;
}


bool ConfigClass::factoryReset()
{
    Preferences prefs;

    if (
        !prefs.begin(
            NVS_NAMESPACE,
            false
        )
    )
    {
        return false;
    }

    const bool cleared =
        prefs.clear();

    prefs.end();

    applyDefaults();

    if (
        LittleFS.exists(
            LEGACY_CONFIG_PATH
        )
    )
    {
        LittleFS.remove(
            LEGACY_CONFIG_PATH
        );
    }

    return cleared;
}
