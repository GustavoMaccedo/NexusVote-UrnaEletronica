// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <cstdio>

#include "core/clock.hpp"
#include "test_framework.hpp"

using nexusvote::hw::isoUtc;
using nexusvote::hw::readSysfsRtc;

TEST(Clock, IsoKnownVectors) {
    EXPECT_EQ(isoUtc(1704067200), "2024-01-01T00:00:00Z");
    EXPECT_EQ(isoUtc(1791849600), "2026-10-13T00:00:00Z");
    EXPECT_EQ(isoUtc(0), "1970-01-01T00:00:00Z");
}

TEST(Clock, MissingRtcReadsZero) {
    EXPECT_EQ(readSysfsRtc("/nao/existe/rtc"), 0ULL);
}

TEST(Clock, MalformedRtcReadsZero) {
    const std::string p = "/tmp/nvclock-bad.txt";
    std::FILE* f = std::fopen(p.c_str(), "w");
    std::fputs("sem-numeros\n", f);
    std::fclose(f);
    EXPECT_EQ(readSysfsRtc(p), 0ULL);
    std::remove(p.c_str());
}

TEST(Clock, SaneRtcParses) {
    const std::string p = "/tmp/nvclock-ok.txt";
    std::FILE* f = std::fopen(p.c_str(), "w");
    std::fputs("1791849600\n", f);
    std::fclose(f);
    EXPECT_EQ(readSysfsRtc(p), 1791849600ULL);
    std::remove(p.c_str());
}

TEST(Clock, NowAlwaysSane) {
    auto t = nexusvote::hw::now();
    EXPECT_TRUE(t.epoch_secs >= nexusvote::hw::kMinSaneEpoch);
}
