#include "DeviceStore.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "Logger.h"

namespace
{
    constexpr const char* DEVICE_FILE =
        "/saved_devices.json";

    constexpr const char* TEMP_FILE =
        "/saved_devices.import.json";

    constexpr const char* BACKUP_FILE =
        "/saved_devices.backup.json";

    constexpr uint32_t FILE_VERSION = 1;

    String normalizeMac(const String& value)
    {
        String normalized = value;
        normalized.trim();
        normalized.toUpperCase();
        return normalized;
    }


    bool parseSixByteHex(
        const String& value,
        bool requireCanonicalFormat)
    {
        if (
            requireCanonicalFormat &&
            value.length() != 17)
        {
            return false;
        }

        if (requireCanonicalFormat)
        {
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
        }

        unsigned int bytes[6];

        const int parsed = sscanf(
            value.c_str(),
            "%x:%x:%x:%x:%x:%x",
            &bytes[0],
            &bytes[1],
            &bytes[2],
            &bytes[3],
            &bytes[4],
            &bytes[5]
        );

        if (parsed != 6)
        {
            return false;
        }

        for (size_t i = 0; i < 6; i++)
        {
            if (bytes[i] > 0xFF)
            {
                return false;
            }
        }

        return true;
    }
}

DeviceStoreClass DeviceStore;


bool DeviceStoreClass::begin()
{
    return load();
}


size_t DeviceStoreClass::getDeviceCount() const
{
    return deviceCount;
}


const Device* DeviceStoreClass::getDevices() const
{
    return devices;
}


const Device* DeviceStoreClass::getById(uint32_t id) const
{
    const int index = findIndexById(id);

    if (index < 0)
    {
        return nullptr;
    }

    return &devices[index];
}


int DeviceStoreClass::findIndexById(uint32_t id) const
{
    for (size_t i = 0; i < deviceCount; i++)
    {
        if (devices[i].id == id)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}


uint32_t DeviceStoreClass::getNextId() const
{
    uint32_t maxId = 0;

    for (size_t i = 0; i < deviceCount; i++)
    {
        if (devices[i].id > maxId)
        {
            maxId = devices[i].id;
        }
    }

    return maxId + 1;
}


bool DeviceStoreClass::macExists(
    const String& mac,
    uint32_t excludeId) const
{
    const String wanted = normalizeMac(mac);

    if (wanted.length() == 0)
    {
        return false;
    }

    for (size_t i = 0; i < deviceCount; i++)
    {
        if (
            devices[i].id != excludeId &&
            normalizeMac(devices[i].mac) == wanted)
        {
            return true;
        }
    }

    return false;
}


bool DeviceStoreClass::validateDevice(
    const Device& device,
    String& errorMessage) const
{
    if (device.name.length() == 0)
    {
        errorMessage = "Device name is required";
        return false;
    }

    if (!parseSixByteHex(device.mac, false))
    {
        errorMessage = "Invalid MAC address";
        return false;
    }

    if (
        device.udpPort == 0 ||
        device.packetCount == 0 ||
        device.packetIntervalMs == 0 ||
        device.maxReachabilityChecks == 0 ||
        device.pingTimeoutMs == 0)
    {
        errorMessage = "Device contains invalid numeric values";
        return false;
    }

    if (
        device.secureOn.length() > 0 &&
        !parseSixByteHex(device.secureOn, true))
    {
        errorMessage = "Invalid SecureOn password";
        return false;
    }

    errorMessage = "";
    return true;
}


bool DeviceStoreClass::add(const Device& inputDevice)
{
    String validationError;

    if (!validateDevice(inputDevice, validationError))
    {
        return false;
    }

    if (deviceCount >= MAX_DEVICES)
    {
        return false;
    }

    if (macExists(inputDevice.mac))
    {
        return false;
    }

    Device device = inputDevice;

    device.id = getNextId();
    device.mac = normalizeMac(device.mac);

    devices[deviceCount] = device;
    deviceCount++;

    if (!save())
    {
        deviceCount--;
        return false;
    }

    return true;
}


bool DeviceStoreClass::update(
    uint32_t id,
    const Device& inputDevice)
{
    String validationError;

    if (!validateDevice(inputDevice, validationError))
    {
        return false;
    }

    const int index = findIndexById(id);

    if (index < 0)
    {
        return false;
    }

    if (macExists(inputDevice.mac, id))
    {
        return false;
    }

    const Device previous = devices[index];
    Device updated = inputDevice;

    updated.id = id;
    updated.mac = normalizeMac(updated.mac);

    // Preserve metadata owned by the store.
    updated.created = previous.created;

    devices[index] = updated;

    if (!save())
    {
        devices[index] = previous;
        return false;
    }

    return true;
}


bool DeviceStoreClass::remove(uint32_t id)
{
    const int index = findIndexById(id);

    if (index < 0)
    {
        return false;
    }

    const Device removed = devices[index];

    for (size_t i = static_cast<size_t>(index);
         i + 1 < deviceCount;
         i++)
    {
        devices[i] = devices[i + 1];
    }

    deviceCount--;

    if (!save())
    {
        for (size_t i = deviceCount;
             i > static_cast<size_t>(index);
             i--)
        {
            devices[i] = devices[i - 1];
        }

        devices[index] = removed;
        deviceCount++;

        return false;
    }

    return true;
}


bool DeviceStoreClass::clear()
{
    deviceCount = 0;

    for (size_t i = 0; i < MAX_DEVICES; ++i)
    {
        devices[i] = Device{};
    }

    const char* files[] = {
        DEVICE_FILE,
        TEMP_FILE,
        BACKUP_FILE
    };

    for (const char* path : files)
    {
        if (
            LittleFS.exists(path) &&
            !LittleFS.remove(path)
        )
        {
            return false;
        }
    }

    return true;
}


bool DeviceStoreClass::load()
{
    deviceCount = 0;

    if (
        LittleFS.exists(DEVICE_FILE) &&
        loadFromFile(DEVICE_FILE)
    )
    {
        if (LittleFS.exists(TEMP_FILE))
        {
            LittleFS.remove(TEMP_FILE);
        }

        return true;
    }

    const bool hadActiveFile =
        LittleFS.exists(DEVICE_FILE);

    if (hadActiveFile)
    {
        Logger.warn(
            "DeviceStore",
            "saved_devices.json is invalid; attempting recovery"
        );
    }

    const char* recoveryFiles[] = {
        BACKUP_FILE,
        TEMP_FILE
    };

    for (const char* recoveryFile : recoveryFiles)
    {
        if (
            !LittleFS.exists(recoveryFile) ||
            !loadFromFile(recoveryFile)
        )
        {
            continue;
        }

        if (
            LittleFS.exists(DEVICE_FILE) &&
            !LittleFS.remove(DEVICE_FILE)
        )
        {
            Logger.error(
                "DeviceStore",
                "recovery loaded but invalid active file could not be removed"
            );

            return true;
        }

        if (!LittleFS.rename(
                recoveryFile,
                DEVICE_FILE))
        {
            Logger.error(
                "DeviceStore",
                "recovery loaded but could not be promoted"
            );

            return true;
        }

        if (LittleFS.exists(TEMP_FILE))
        {
            LittleFS.remove(TEMP_FILE);
        }

        Logger.warn(
            "DeviceStore",
            "saved_devices.json recovered successfully"
        );

        return true;
    }

    if (!hadActiveFile)
    {
        return
            !LittleFS.exists(BACKUP_FILE) &&
            !LittleFS.exists(TEMP_FILE);
    }

    return false;
}


bool DeviceStoreClass::loadFromFile(
    const char* path)
{
    deviceCount = 0;

    File file =
        LittleFS.open(path, "r");

    if (!file)
    {
        return false;
    }

    JsonDocument doc;

    const DeserializationError error =
        deserializeJson(doc, file);

    file.close();

    if (error)
    {
        return false;
    }

    String validationError;

    if (
        !migrateImportedConfig(
            doc,
            validationError) ||
        !validateImportedConfig(
            doc,
            validationError))
    {
        return false;
    }

    JsonArray array =
        doc["devices"].as<JsonArray>();

    for (JsonObject obj : array)
    {
        Device& device =
            devices[deviceCount];

        device.id =
            obj["id"] | 0;

        device.enabled =
            obj["enabled"] | true;

        device.wakeOnBoot =
            obj["wakeOnBoot"] | true;

        device.name =
            obj["name"] | "";

        device.mac =
            normalizeMac(obj["mac"] | "");

        device.ip =
            obj["ip"] | "";

        device.udpPort =
            obj["udpPort"] | 9;

        device.initialDelayMs =
            obj["initialDelayMs"] | 30000;

        device.packetCount =
            obj["packetCount"] | 5;

        device.packetIntervalMs =
            obj["packetIntervalMs"] | 30000;

        device.stopWhenReachable =
            obj["stopWhenReachable"] | true;

        device.maxReachabilityChecks =
            obj["maxReachabilityChecks"] | 20;

        device.pingTimeoutMs =
            obj["pingTimeoutMs"] | 500;

        device.broadcast =
            obj["broadcast"] | "";

        device.secureOn =
            obj["secureOn"] | "";

        device.category =
            obj["category"] | "";

        device.notes =
            obj["notes"] | "";

        device.created =
            obj["created"] | 0;

        device.updated =
            obj["updated"] | 0;

        deviceCount++;
    }

    return true;
}



bool DeviceStoreClass::exportConfiguration(
    String& json) const
{
    JsonDocument doc;

    doc["version"] = FILE_VERSION;

    JsonArray array =
        doc["devices"].to<JsonArray>();

    for (size_t i = 0; i < deviceCount; i++)
    {
        const Device& device =
            devices[i];

        JsonObject obj =
            array.add<JsonObject>();

        obj["id"] = device.id;
        obj["enabled"] = device.enabled;
        obj["wakeOnBoot"] = device.wakeOnBoot;
        obj["name"] = device.name;
        obj["mac"] = device.mac;
        obj["ip"] = device.ip;
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

    json = "";
    serializeJsonPretty(doc, json);

    return json.length() > 0;
}


bool DeviceStoreClass::migrateImportedConfig(
    JsonDocument& doc,
    String& errorMessage) const
{
    const uint32_t importedVersion =
        doc["version"] | 1;

    if (importedVersion > FILE_VERSION)
    {
        errorMessage =
            "Configuration file was created by a newer WakeWizard version";

        return false;
    }

    /*
     * Migration policy:
     * configuration schemas evolve by adding fields.
     *
     * The import path always applies current defaults to fields
     * that are missing. Future version-specific migrations can be
     * inserted here before these defaults are applied.
     */
    JsonArray array =
        doc["devices"].as<JsonArray>();

    if (array.isNull())
    {
        errorMessage =
            "Configuration does not contain a devices array";

        return false;
    }

    uint32_t nextGeneratedId = 1;

    for (JsonObject obj : array)
    {
        if (!obj["id"].is<uint32_t>())
        {
            obj["id"] = nextGeneratedId;
        }

        const uint32_t currentId =
            obj["id"] | nextGeneratedId;

        if (currentId >= nextGeneratedId)
        {
            nextGeneratedId = currentId + 1;
        }

        if (!obj["enabled"].is<bool>())
        {
            obj["enabled"] = true;
        }

        if (!obj["wakeOnBoot"].is<bool>())
        {
            obj["wakeOnBoot"] = true;
        }

        if (!obj["ip"].is<const char*>())
        {
            obj["ip"] = "";
        }

        if (!obj["udpPort"].is<uint16_t>())
        {
            obj["udpPort"] = 9;
        }

        if (!obj["initialDelayMs"].is<uint32_t>())
        {
            obj["initialDelayMs"] = 30000;
        }

        if (!obj["packetCount"].is<uint16_t>())
        {
            obj["packetCount"] = 5;
        }

        if (!obj["packetIntervalMs"].is<uint32_t>())
        {
            obj["packetIntervalMs"] = 30000;
        }

        if (!obj["stopWhenReachable"].is<bool>())
        {
            obj["stopWhenReachable"] = true;
        }

        if (!obj["maxReachabilityChecks"].is<uint16_t>())
        {
            obj["maxReachabilityChecks"] = 20;
        }

        if (!obj["pingTimeoutMs"].is<uint32_t>())
        {
            obj["pingTimeoutMs"] = 500;
        }

        if (!obj["broadcast"].is<const char*>())
        {
            obj["broadcast"] = "";
        }

        if (!obj["secureOn"].is<const char*>())
        {
            obj["secureOn"] = "";
        }

        if (!obj["category"].is<const char*>())
        {
            obj["category"] = "";
        }

        if (!obj["notes"].is<const char*>())
        {
            obj["notes"] = "";
        }

        if (!obj["created"].is<uint32_t>())
        {
            obj["created"] = 0;
        }

        if (!obj["updated"].is<uint32_t>())
        {
            obj["updated"] = 0;
        }
    }

    doc["version"] = FILE_VERSION;

    return true;
}


bool DeviceStoreClass::validateImportedConfig(
    const JsonDocument& doc,
    String& errorMessage) const
{
    JsonArrayConst array =
        doc["devices"].as<JsonArrayConst>();

    if (array.isNull())
    {
        errorMessage =
            "Configuration does not contain a devices array";

        return false;
    }

    if (array.size() > MAX_DEVICES)
    {
        errorMessage =
            "Configuration contains too many devices";

        return false;
    }

    String seenMacs[MAX_DEVICES];
    uint32_t seenIds[MAX_DEVICES];
    size_t seenCount = 0;

    for (JsonObjectConst obj : array)
    {
        const uint32_t id =
            obj["id"] | 0;

        if (id == 0)
        {
            errorMessage =
                "Every device must have a valid id";

            return false;
        }

        Device device{};
        device.name = obj["name"] | "";
        device.mac = obj["mac"] | "";
        device.udpPort = obj["udpPort"] | 0;
        device.packetCount = obj["packetCount"] | 0;
        device.packetIntervalMs =
            obj["packetIntervalMs"] | 0;
        device.maxReachabilityChecks =
            obj["maxReachabilityChecks"] | 0;
        device.pingTimeoutMs =
            obj["pingTimeoutMs"] | 0;
        device.secureOn = obj["secureOn"] | "";

        if (!validateDevice(device, errorMessage))
        {
            return false;
        }

        const String mac =
            normalizeMac(device.mac);

        for (size_t i = 0; i < seenCount; i++)
        {
            if (seenIds[i] == id)
            {
                errorMessage =
                    "Configuration contains duplicate device ids";

                return false;
            }

            if (seenMacs[i] == mac)
            {
                errorMessage =
                    "Configuration contains duplicate MAC addresses";

                return false;
            }
        }

        seenIds[seenCount] = id;
        seenMacs[seenCount] = mac;
        seenCount++;
    }

    return true;
}


bool DeviceStoreClass::validatePersistedFile(
    const char* path,
    size_t expectedBytes,
    size_t expectedDeviceCount) const
{
    File file =
        LittleFS.open(path, "r");

    if (!file)
    {
        return false;
    }

    if (file.size() != expectedBytes)
    {
        file.close();
        return false;
    }

    JsonDocument doc;

    const DeserializationError error =
        deserializeJson(doc, file);

    file.close();

    if (error)
    {
        return false;
    }

    const JsonArrayConst array =
        doc["devices"].as<JsonArrayConst>();

    return
        !array.isNull() &&
        array.size() == expectedDeviceCount &&
        (doc["version"] | 0) == FILE_VERSION;
}


bool DeviceStoreClass::persistDocument(
    const JsonDocument& doc,
    String& errorMessage)
{
    if (
        LittleFS.exists(TEMP_FILE) &&
        !LittleFS.remove(TEMP_FILE)
    )
    {
        errorMessage =
            "Unable to remove stale temporary configuration file";
        return false;
    }

    File temp =
        LittleFS.open(
            TEMP_FILE,
            "w"
        );

    if (!temp)
    {
        errorMessage =
            "Unable to create temporary configuration file";
        return false;
    }

    const size_t expectedBytes =
        measureJsonPretty(doc);

    const size_t bytesWritten =
        serializeJsonPretty(
            doc,
            temp
        );

    temp.close();

    const size_t expectedDeviceCount =
        doc["devices"].as<JsonArrayConst>().size();

    if (
        expectedBytes == 0 ||
        bytesWritten != expectedBytes ||
        !validatePersistedFile(
            TEMP_FILE,
            expectedBytes,
            expectedDeviceCount
        )
    )
    {
        LittleFS.remove(TEMP_FILE);

        errorMessage =
            "Temporary configuration file validation failed";
        return false;
    }

    if (
        LittleFS.exists(BACKUP_FILE) &&
        !LittleFS.remove(BACKUP_FILE)
    )
    {
        LittleFS.remove(TEMP_FILE);

        errorMessage =
            "Unable to remove previous configuration backup";
        return false;
    }

    const bool hadExistingFile =
        LittleFS.exists(DEVICE_FILE);

    if (
        hadExistingFile &&
        !LittleFS.rename(
            DEVICE_FILE,
            BACKUP_FILE)
    )
    {
        LittleFS.remove(TEMP_FILE);

        errorMessage =
            "Unable to create configuration backup";
        return false;
    }

    if (!LittleFS.rename(
            TEMP_FILE,
            DEVICE_FILE))
    {
        if (hadExistingFile)
        {
            LittleFS.rename(
                BACKUP_FILE,
                DEVICE_FILE
            );
        }

        errorMessage =
            "Unable to activate configuration";
        return false;
    }

    if (!validatePersistedFile(
            DEVICE_FILE,
            expectedBytes,
            expectedDeviceCount))
    {
        LittleFS.remove(DEVICE_FILE);

        if (hadExistingFile)
        {
            LittleFS.rename(
                BACKUP_FILE,
                DEVICE_FILE
            );
        }

        errorMessage =
            "Activated configuration validation failed";
        return false;
    }

    errorMessage = "";
    return true;
}


bool DeviceStoreClass::importConfiguration(
    const String& json,
    String& errorMessage)
{
    JsonDocument doc;

    const DeserializationError parseError =
        deserializeJson(doc, json);

    if (parseError)
    {
        errorMessage =
            "Invalid JSON configuration";

        return false;
    }

    JsonArrayConst importedDevices =
        doc["devices"].as<JsonArrayConst>();

    Logger.info(
        "ConfigImport",
        "bodyBytes=" + String(json.length()) +
        "; version=" +
        String(
            static_cast<uint32_t>(
                doc["version"] | 1
            )
        ) +
        "; devicesArray=" +
        String(
            importedDevices.isNull()
                ? "NO"
                : "YES"
        ) +
        "; deviceCount=" +
        String(
            importedDevices.isNull()
                ? 0
                : importedDevices.size()
        )
    );

    if (!migrateImportedConfig(
            doc,
            errorMessage))
    {
        return false;
    }

    if (!validateImportedConfig(
            doc,
            errorMessage))
    {
        return false;
    }

    if (!persistDocument(
            doc,
            errorMessage))
    {
        return false;
    }

    if (!loadFromFile(DEVICE_FILE))
    {
        LittleFS.remove(DEVICE_FILE);

        if (LittleFS.exists(BACKUP_FILE))
        {
            LittleFS.rename(
                BACKUP_FILE,
                DEVICE_FILE
            );

            load();
        }

        errorMessage =
            "Imported configuration could not be loaded";

        return false;
    }

    return true;
}


bool DeviceStoreClass::save()
{
    JsonDocument doc;

    doc["version"] = FILE_VERSION;

    JsonArray array =
        doc["devices"].to<JsonArray>();

    for (size_t i = 0; i < deviceCount; i++)
    {
        const Device& device =
            devices[i];

        JsonObject obj =
            array.add<JsonObject>();

        obj["id"] = device.id;
        obj["enabled"] = device.enabled;
        obj["wakeOnBoot"] = device.wakeOnBoot;
        obj["name"] = device.name;
        obj["mac"] = device.mac;
        obj["ip"] = device.ip;
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

    String errorMessage;

    if (!persistDocument(
            doc,
            errorMessage))
    {
        Logger.error(
            "DeviceStore",
            "save failed: " + errorMessage
        );

        return false;
    }

    return true;
}
