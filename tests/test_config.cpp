// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <fstream>
#include <sstream>

#include "../src/core/config_loader.hpp"
#include "test_framework.hpp"

namespace {

std::string demoConfig() {
    std::ifstream f(CONFIG_DIR "/demo-election.json", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

TEST(ConfigValidation, DemoIsValid) {
    auto r = nexusvote::loadConfigFromText(demoConfig());
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.config.offices.size(), 2u);
    EXPECT_EQ(r.config.candidates.size(), 5u);
}

TEST(ConfigValidation, RejectsBadJson) {
    auto r = nexusvote::loadConfigFromText("{invalido");
    EXPECT_FALSE(r.ok);
}

TEST(ConfigValidation, RejectsWrongSchema) {
    std::string t = demoConfig();
    auto pos = t.find("\"schema_version\": 1");
    EXPECT_TRUE(pos != std::string::npos);
    t.replace(pos, 18, "\"schema_version\": 9");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}

TEST(ConfigValidation, RejectsDuplicateNumber) {
    std::string t = demoConfig();
    auto pos = t.find("\"number\": \"11222\"");
    EXPECT_TRUE(pos != std::string::npos);
    t.replace(pos + 11, 5, "11111");
    auto r = nexusvote::loadConfigFromText(t);
    EXPECT_FALSE(r.ok);
}

TEST(ConfigValidation, RejectsDuplicateCandidateId) {
    std::string t = demoConfig();
    auto pos = t.find("\"candidate_id\": \"c-beto\"");
    EXPECT_TRUE(pos != std::string::npos);
    t.replace(pos + 17, 6, "c-ada");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}

TEST(ConfigValidation, RejectsEmptyOffices) {
    auto r = nexusvote::loadConfigFromText(
        "{\"schema_version\":1,\"election_id\":\"X\",\"display_name\":\"Y\","
        "\"offices\":[],\"candidates\":[]}");
    EXPECT_FALSE(r.ok);
}

TEST(ConfigValidation, RejectsBadNumberLength) {
    std::string t = demoConfig();
    auto pos = t.find("\"number_length\": 5");
    EXPECT_TRUE(pos != std::string::npos);
    t.replace(pos + 17, 1, "9");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}

TEST(ConfigValidation, RejectsUnknownOffice) {
    std::string t = demoConfig();
    auto pos = t.find("\"office_id\": \"vereador\", \"number\": \"11111\"");
    EXPECT_TRUE(pos != std::string::npos);
    t.replace(pos + 14, 8, "prefeito");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}

TEST(ConfigLimits, AcceptsValidLimits) {
    std::string t = demoConfig();
    auto pos = t.find("\"legend_enabled\": true");
    EXPECT_TRUE(pos != std::string::npos);
    t.insert(pos, "\"test_max_voters\": 2, \"test_until_iso\": \"2099-01-01T00:00:00Z\", ");
    auto r = nexusvote::loadConfigFromText(t);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.config.test_max_voters, 2);
    EXPECT_EQ(r.config.test_until_iso, "2099-01-01T00:00:00Z");
}

TEST(ConfigLimits, RejectsNegativeMax) {
    std::string t = demoConfig();
    auto pos = t.find("\"legend_enabled\": true");
    EXPECT_TRUE(pos != std::string::npos);
    t.insert(pos, "\"test_max_voters\": -1, ");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}

TEST(ConfigLimits, RejectsBadUntilFormat) {
    std::string t = demoConfig();
    auto pos = t.find("\"legend_enabled\": true");
    EXPECT_TRUE(pos != std::string::npos);
    t.insert(pos, "\"test_until_iso\": \"amanha\", ");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}

TEST(ConfigMates, ParsesRunningMateAndSubstitutes) {
    std::string t = demoConfig();
    auto pos = t.find("\"candidate_id\": \"c-ada\"");
    EXPECT_TRUE(pos != std::string::npos);
    t.insert(pos,
             "\"running_mate\": \"Vice Ficticia\", \"substitutes\": [\"Sup Um\", \"Sup Dois\"], ");
    auto r = nexusvote::loadConfigFromText(t);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.config.candidates[0].running_mate, "Vice Ficticia");
    EXPECT_EQ(r.config.candidates[0].substitutes.size(), 2u);
}

TEST(ConfigMates, RejectsThreeSubstitutes) {
    std::string t = demoConfig();
    auto pos = t.find("\"candidate_id\": \"c-ada\"");
    EXPECT_TRUE(pos != std::string::npos);
    t.insert(pos, "\"substitutes\": [\"A\", \"B\", \"C\"], ");
    EXPECT_FALSE(nexusvote::loadConfigFromText(t).ok);
}
