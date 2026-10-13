// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <cstdint>
#include <string>

namespace nexusvote::hw {

inline constexpr std::uint64_t kMinSaneEpoch = 1704067200;
inline constexpr const char* kRtcSysfsPath = "/sys/class/rtc/rtc0/since_epoch";

struct TimeReading {
    std::uint64_t epoch_secs = 0;
    bool trusted = false;
};

std::uint64_t readSysfsRtc(const std::string& path = kRtcSysfsPath);

TimeReading now();

std::uint64_t systemEpoch();

std::string isoUtc(std::uint64_t epoch_secs);

} // namespace nexusvote::hw
