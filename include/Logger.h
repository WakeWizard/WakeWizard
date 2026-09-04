#pragma once

#include <Arduino.h>
#include <time.h>

class LoggerClass
{
public:
    enum class Level
    {
        Info,
        Warn,
        Error
    };

    bool begin();
    void startTimeSync();
    void loop();

    void info(const char* component, const String& message);
    void warn(const char* component, const String& message);
    void error(const char* component, const String& message);

    String getCurrentFileName() const;
    uint32_t getMaxFileBytes() const;
    uint32_t getPersistedLineCount() const;
    uint32_t getWriteFailureCount() const;

    bool isTimeSyncRequested() const;
    bool isTimeSynchronized() const;
    String getCurrentUtcTime() const;
    String getNtpServers() const;

    bool isValidLogFileName(const String& fileName) const;
    bool deleteLogFile(
        const String& fileName,
        String& errorMessage
    );
    size_t deleteHistory();

private:
    static constexpr uint32_t CLOCK_CHECK_INTERVAL_MS = 10000;
    static constexpr uint32_t ABSOLUTE_MAX_FILE_BYTES = 128UL * 1024UL;
    static constexpr time_t VALID_TIME_THRESHOLD = 1700000000;

    bool initialized = false;
    bool fileLimitWarningPrinted = false;
    bool ntpRequested = false;

    uint32_t maxFileBytes = ABSOLUTE_MAX_FILE_BYTES;
    uint32_t nextClockCheckMs = 0;
    uint32_t persistedLineCount = 0;
    uint32_t writeFailureCount = 0;

    String currentDate;
    String currentPath;

    void log(Level level, const char* component, const String& message);
    bool appendToFile(const String& line);

    bool hasValidTime() const;
    bool updateCurrentLogFile();
    String formatTimestamp() const;
    String dateForTime(time_t value) const;
    const char* levelName(Level level) const;

    void pruneOldLogs();
    bool isDailyLogFileName(const String& fileName) const;
};

extern LoggerClass Logger;
