#include "Logger.h"

#include <LittleFS.h>
#include <time.h>
#include <cstring>

#include "Config.h"

namespace
{
    constexpr const char* LOG_DIRECTORY = "/logs";
    constexpr const char* UNSYNCED_LOG = "/logs/wakewizard-unsynced.log";
    constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
    constexpr const char* NTP_SERVER_2 = "time.nist.gov";

    bool timeReached(uint32_t now, uint32_t target)
    {
        return static_cast<int32_t>(now - target) >= 0;
    }
}

LoggerClass Logger;


bool LoggerClass::begin()
{
    if (initialized)
    {
        return true;
    }

    if (!LittleFS.exists(LOG_DIRECTORY))
    {
        if (!LittleFS.mkdir(LOG_DIRECTORY))
        {
            Serial.println("Logger: unable to create /logs directory");
            return false;
        }
    }

    const uint8_t retentionDays =
        Config.getLogRetentionDays() > 0
            ? Config.getLogRetentionDays()
            : 1;

    const size_t totalBytes = LittleFS.totalBytes();

    if (totalBytes > 0)
    {
        const uint32_t budget =
            static_cast<uint32_t>(
                (static_cast<uint64_t>(totalBytes) * 70ULL) /
                (100ULL * retentionDays)
            );

        if (budget < maxFileBytes)
        {
            maxFileBytes = budget;
        }
    }


    initialized = true;
    currentPath = UNSYNCED_LOG;

    updateCurrentLogFile();
    pruneOldLogs();

    nextClockCheckMs =
        millis() + CLOCK_CHECK_INTERVAL_MS;

    info(
        "Logger",
        "ready; retention=" +
        String(Config.getLogRetentionDays()) +
        " day(s); maxFileBytes=" +
        String(maxFileBytes)
    );

    return true;
}


void LoggerClass::startTimeSync()
{
    if (!initialized || ntpRequested)
    {
        return;
    }

    ntpRequested = true;

    // NTP synchronization is deliberately non-blocking: logging must
    // never delay the Wake-on-Boot scheduling path. Startup messages
    // remain in wakewizard-unsynced.log until the clock becomes valid.
    configTime(
        0,
        0,
        NTP_SERVER_1,
        NTP_SERVER_2
    );

    info(
        "Logger",
        "NTP synchronization requested"
    );
}


void LoggerClass::loop()
{
    if (!initialized)
    {
        return;
    }

    const uint32_t now = millis();

    if (!timeReached(now, nextClockCheckMs))
    {
        return;
    }

    nextClockCheckMs =
        now + CLOCK_CHECK_INTERVAL_MS;

    if (updateCurrentLogFile())
    {
        fileLimitWarningPrinted = false;
        pruneOldLogs();

        info(
            "Logger",
            "daily log file selected: " +
            getCurrentFileName()
        );
    }
}


void LoggerClass::info(
    const char* component,
    const String& message)
{
    log(Level::Info, component, message);
}


void LoggerClass::warn(
    const char* component,
    const String& message)
{
    log(Level::Warn, component, message);
}


void LoggerClass::error(
    const char* component,
    const String& message)
{
    log(Level::Error, component, message);
}


void LoggerClass::log(
    Level level,
    const char* component,
    const String& message)
{
    String line;
    line.reserve(
        message.length() +
        strlen(component) +
        40
    );

    line += formatTimestamp();
    line += " ";
    line += levelName(level);
    line += " ";
    line += component;
    line += " | ";
    line += message;

    // Every wakewizard application log is mirrored to Serial.
    Serial.println(line);

    if (initialized)
    {
        if (updateCurrentLogFile())
        {
            fileLimitWarningPrinted = false;
            pruneOldLogs();
        }

        if (appendToFile(line))
        {
            persistedLineCount++;
        }
        else
        {
            writeFailureCount++;
        }
    }
}


bool LoggerClass::appendToFile(
    const String& line)
{
    if (currentPath.length() == 0)
    {
        return false;
    }

    size_t existingSize = 0;

    if (LittleFS.exists(currentPath))
    {
        File existing =
            LittleFS.open(currentPath, "r");

        if (existing)
        {
            existingSize = existing.size();
            existing.close();
        }
    }

    if (
        existingSize + line.length() + 1 >
        maxFileBytes
    )
    {
        if (!fileLimitWarningPrinted)
        {
            fileLimitWarningPrinted = true;

            Serial.print(
                "Logger: log file size limit reached for "
            );
            Serial.println(currentPath);
        }

        return false;
    }

    File file =
        LittleFS.open(currentPath, "a");

    if (!file)
    {
        Serial.print("Logger: unable to open ");
        Serial.println(currentPath);
        return false;
    }

    const size_t written =
        file.println(line);

    file.close();

    if (written == 0)
    {
        Serial.print(
            "Logger: write failed for "
        );
        Serial.println(currentPath);
        return false;
    }

    return true;
}


uint32_t LoggerClass::getPersistedLineCount() const
{
    return persistedLineCount;
}


uint32_t LoggerClass::getWriteFailureCount() const
{
    return writeFailureCount;
}


bool LoggerClass::isTimeSyncRequested() const
{
    return ntpRequested;
}


bool LoggerClass::isTimeSynchronized() const
{
    return hasValidTime();
}


String LoggerClass::getCurrentUtcTime() const
{
    if (!hasValidTime())
    {
        return "";
    }

    const time_t now =
        time(nullptr);

    struct tm timeInfo;
    gmtime_r(
        &now,
        &timeInfo
    );

    char buffer[24];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );

    return String(buffer);
}


String LoggerClass::getNtpServers() const
{
    return
        String(NTP_SERVER_1) +
        ", " +
        NTP_SERVER_2;
}


bool LoggerClass::hasValidTime() const
{
    return time(nullptr) >= VALID_TIME_THRESHOLD;
}


bool LoggerClass::updateCurrentLogFile()
{
    String nextDate;
    String nextPath;

    if (hasValidTime())
    {
        nextDate = dateForTime(time(nullptr));
        nextPath =
            String(LOG_DIRECTORY) +
            "/wakewizard-" +
            nextDate +
            ".log";
    }
    else
    {
        nextDate = "";
        nextPath = UNSYNCED_LOG;
    }

    if (nextPath == currentPath)
    {
        return false;
    }

    currentDate = nextDate;
    currentPath = nextPath;

    return true;
}


String LoggerClass::formatTimestamp() const
{
    if (!hasValidTime())
    {
        const uint32_t ms = millis();

        return
            "BOOT+" +
            String(ms / 1000) +
            "." +
            String(ms % 1000);
    }

    const time_t now = time(nullptr);
    struct tm timeInfo;
    gmtime_r(&now, &timeInfo);

    char buffer[24];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );

    return String(buffer);
}


String LoggerClass::dateForTime(time_t value) const
{
    struct tm timeInfo;
    gmtime_r(&value, &timeInfo);

    char buffer[11];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d",
        &timeInfo
    );

    return String(buffer);
}


const char* LoggerClass::levelName(Level level) const
{
    switch (level)
    {
        case Level::Warn:
            return "WARN";

        case Level::Error:
            return "ERROR";

        case Level::Info:
        default:
            return "INFO";
    }
}


String LoggerClass::getCurrentFileName() const
{
    const int slash =
        currentPath.lastIndexOf('/');

    if (slash < 0)
    {
        return currentPath;
    }

    return currentPath.substring(slash + 1);
}


uint32_t LoggerClass::getMaxFileBytes() const
{
    return maxFileBytes;
}


bool LoggerClass::isDailyLogFileName(
    const String& fileName) const
{
    // wakewizard-YYYY-MM-DD.log = 25 characters
    //
    // 0123456789012345678901234
    // wakewizard-2026-09-02.log
    //
    // Date starts at index 11.
    if (
        fileName.length() != 25 ||
        !fileName.startsWith("wakewizard-") ||
        !fileName.endsWith(".log")
    )
    {
        return false;
    }

    // YYYY-MM-DD separators.
    if (
        fileName[15] != '-' ||
        fileName[18] != '-'
    )
    {
        return false;
    }

    // Validate all date characters, not just the separators.
    for (size_t i = 11; i <= 20; ++i)
    {
        if (i == 15 || i == 18)
        {
            continue;
        }

        if (!isDigit(fileName[i]))
        {
            return false;
        }
    }

    return true;
}


bool LoggerClass::isValidLogFileName(
    const String& fileName) const
{
    if (
        fileName.indexOf('/') >= 0 ||
        fileName.indexOf("..") >= 0
    )
    {
        return false;
    }

    return
        isDailyLogFileName(fileName) ||
        fileName == "wakewizard-unsynced.log";
}


bool LoggerClass::deleteLogFile(
    const String& fileName,
    String& errorMessage)
{
    if (!isValidLogFileName(fileName))
    {
        errorMessage = "Invalid log file name";
        return false;
    }

    const String path =
        String(LOG_DIRECTORY) + "/" + fileName;

    if (!LittleFS.exists(path))
    {
        errorMessage = "Log file not found";
        return false;
    }

    if (!LittleFS.remove(path))
    {
        errorMessage = "Unable to delete log file";
        return false;
    }

    if (fileName == getCurrentFileName())
    {
        fileLimitWarningPrinted = false;
    }

    errorMessage = "";
    return true;
}


size_t LoggerClass::deleteHistory()
{
    size_t deleted = 0;
    const String currentFile = getCurrentFileName();

    File directory = LittleFS.open(LOG_DIRECTORY);

    if (!directory || !directory.isDirectory())
    {
        return 0;
    }

    String filesToDelete[32];
    size_t count = 0;

    File file = directory.openNextFile();

    while (file)
    {
        String fileName = file.name();

        const int slash = fileName.lastIndexOf('/');

        if (slash >= 0)
        {
            fileName = fileName.substring(slash + 1);
        }

        if (
            isValidLogFileName(fileName) &&
            fileName != currentFile &&
            count < 32
        )
        {
            filesToDelete[count++] = fileName;
        }

        file.close();
        file = directory.openNextFile();
    }

    directory.close();

    for (size_t i = 0; i < count; i++)
    {
        const String path =
            String(LOG_DIRECTORY) + "/" + filesToDelete[i];

        if (LittleFS.remove(path))
        {
            deleted++;
        }
    }

    return deleted;
}


void LoggerClass::pruneOldLogs()
{
    const uint8_t retentionDays =
        Config.getLogRetentionDays() > 0
            ? Config.getLogRetentionDays()
            : 1;

    String names[31];
    size_t count = 0;

    File directory =
        LittleFS.open(LOG_DIRECTORY);

    if (!directory || !directory.isDirectory())
    {
        return;
    }

    File file = directory.openNextFile();

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

        if (
            isDailyLogFileName(fileName) &&
            count < 31
        )
        {
            names[count++] = fileName;
        }

        file.close();
        file = directory.openNextFile();
    }

    directory.close();

    // ISO date filenames sort chronologically as strings.
    for (size_t i = 0; i < count; i++)
    {
        for (size_t j = i + 1; j < count; j++)
        {
            if (names[j] < names[i])
            {
                const String temp = names[i];
                names[i] = names[j];
                names[j] = temp;
            }
        }
    }

    while (count > retentionDays)
    {
        const String path =
            String(LOG_DIRECTORY) +
            "/" +
            names[0];

        LittleFS.remove(path);

        for (size_t i = 1; i < count; i++)
        {
            names[i - 1] = names[i];
        }

        count--;
    }
}
