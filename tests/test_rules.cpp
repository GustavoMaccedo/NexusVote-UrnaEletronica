// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <fstream>
#include <sstream>

#include "../src/core/config_loader.hpp"
#include "../src/core/rules.hpp"
#include "test_framework.hpp"

namespace {

nexusvote::ElectionConfig cfg() {
    std::ifstream f(CONFIG_DIR "/demo-election.json", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    auto r = nexusvote::loadConfigFromText(ss.str());
    if (!r.ok)
        std::abort();
    return r.config;
}

nexusvote::ElectionRules rulesOn() {
    nexusvote::ElectionRules rl;
    rl.legend_enabled = true;
    return rl;
}

} // namespace

TEST(Rules, NominalValid) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "vereador", "11111", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Nominal);
    EXPECT_EQ(pv.candidate_id, "c-ada");
}

TEST(Rules, PartialIsIncomplete) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "vereador", "111", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Incomplete);
}

TEST(Rules, UnknownNumberIsNull) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "alcaide", "99", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Null);
}

TEST(Rules, BlankExplicit) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "alcaide", "", true);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Blank);
}

TEST(Rules, NonNumericIsInvalid) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "alcaide", "1a", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Invalid);
}

TEST(Rules, OverlongIsIncompleteOrInvalid) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "alcaide", "123", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Incomplete ||
                pv.type == nexusvote::PreviewType::Null);
}

TEST(Rules, LegendWhenEnabled) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "vereador", "11999", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Legend);
    EXPECT_EQ(pv.party_number, "11");
}

TEST(Rules, LegendWhenDisabledIsNull) {
    auto c = cfg();
    nexusvote::ElectionRules off;
    off.legend_enabled = false;
    auto pv = nexusvote::classifyBallot(c, off, "vereador", "11999", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Null);
}

TEST(Rules, LegendNotForMajoritarian) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "alcaide", "19", false);
    EXPECT_TRUE(pv.type != nexusvote::PreviewType::Legend);
}

TEST(Rules, UnknownOfficeIsInvalid) {
    auto c = cfg();
    auto pv = nexusvote::classifyBallot(c, rulesOn(), "inexistente", "11", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Invalid);
}
