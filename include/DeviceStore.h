#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Device.h"

class DeviceStoreClass
{
public:
    bool begin();

    size_t getDeviceCount() const;
    const Device* getDevices() const;
    const Device* getById(uint32_t id) const;

    bool add(const Device& device);
    bool update(uint32_t id, const Device& device);
    bool remove(uint32_t id);
    bool clear();

    bool exportConfiguration(String& json) const;
    bool importConfiguration(
        const String& json,
        String& errorMessage
    );

    bool validateDevice(
        const Device& device,
        String& errorMessage
    ) const;

    bool macExists(const String& mac, uint32_t excludeId = 0) const;

private:
    static constexpr size_t MAX_DEVICES = 32;

    Device devices[MAX_DEVICES];
    size_t deviceCount = 0;

    bool load();
    bool loadFromFile(const char* path);
    bool save();
    bool persistDocument(
        const JsonDocument& doc,
        String& errorMessage
    );
    bool validatePersistedFile(
        const char* path,
        size_t expectedBytes,
        size_t expectedDeviceCount
    ) const;

    bool migrateImportedConfig(
        JsonDocument& doc,
        String& errorMessage
    ) const;

    bool validateImportedConfig(
        const JsonDocument& doc,
        String& errorMessage
    ) const;

    int findIndexById(uint32_t id) const;
    uint32_t getNextId() const;
};

extern DeviceStoreClass DeviceStore;
