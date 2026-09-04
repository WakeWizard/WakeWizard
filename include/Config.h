#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class ConfigClass
{
public:
    bool begin();
    bool save();

    const char* getWifiSSID() const;
    const char* getWifiPassword() const;
    const char* getHostname() const;
    const char* getFirmwareVersion() const;
    const char* getLanguage() const;
    uint8_t getLogRetentionDays() const;

    bool isProvisioned() const;
    bool verifyAdminPassword(const String& password) const;

    void setWifiSSID(const String& value);
    void setWifiPassword(const String& value);
    void setHostname(const String& value);
    void setLanguage(const String& value);
    void setLogRetentionDays(uint8_t value);
    bool setAdminPassword(const String& password);

    bool exportConfiguration(String& json) const;
    bool toPublicJson(String& json) const;
    bool importConfiguration(const String& json, String& errorMessage);
    bool factoryReset();

private:
    static constexpr uint32_t FILE_VERSION = 1;
    static constexpr uint8_t DEFAULT_LOG_RETENTION_DAYS = 8;
    static constexpr const char* DEFAULT_LANGUAGE = "en";

    String wifiSSID;
    String wifiPassword;
    String hostname = "wakewizard";
    String language = DEFAULT_LANGUAGE;
    String adminSalt;
    String adminPasswordHash;
    uint8_t logRetentionDays = DEFAULT_LOG_RETENTION_DAYS;
    bool provisioned = false;

    void applyDefaults();
    bool loadFromNvs();
    bool migrateLegacyLittleFsConfig();
    bool migrateImportedConfig(JsonDocument& doc, String& errorMessage) const;

    void recomputeProvisioned();

    static String makeSalt();
    static String hashPassword(const String& password, const String& salt);
};

extern ConfigClass Config;
