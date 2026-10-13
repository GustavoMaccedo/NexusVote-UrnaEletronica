// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "core/clock.hpp"

#include <cstdio>
#include <ctime>

namespace nexusvote::hw {

std::uint64_t readSysfsRtc(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (f == nullptr) {
        return 0;
    }
    unsigned long long v = 0;
    const int n = std::fscanf(f, "%llu", &v);
    std::fclose(f);
    if (n != 1) {
        return 0;
    }
    return static_cast<std::uint64_t>(v);
}

std::uint64_t systemEpoch() {
    const std::time_t t = std::time(nullptr);
    return t < 0 ? 0 : static_cast<std::uint64_t>(t);
}

TimeReading now() {
    const std::uint64_t rtc = readSysfsRtc();
    if (rtc >= kMinSaneEpoch) {
        return TimeReading{rtc, true};
    }
    return TimeReading{systemEpoch(), false};
}

std::string isoUtc(std::uint64_t epoch_secs) {
    const long long days = static_cast<long long>(epoch_secs / 86400);
    const long long secs = static_cast<long long>(epoch_secs % 86400);
    const long long z = days + 719468;
    const long long era = z >= 0 ? z / 146097 : (z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long y = yoe + era * 400;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const long long d = doy - (153 * mp + 2) / 5 + 1;
    const long long m = mp < 10 ? mp + 3 : mp - 9;
    const long long year = m <= 2 ? y + 1 : y;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04lld-%02lld-%02lldT%02lld:%02lld:%02lldZ", year, m, d,
                  secs / 3600, (secs % 3600) / 60, secs % 60);
    return std::string(buf);
}

} // namespace nexusvote::hw
