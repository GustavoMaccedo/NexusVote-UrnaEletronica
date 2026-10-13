// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <filesystem>
#include <fstream>
#include <sstream>

#include "../src/core/config_loader.hpp"
#include "../src/core/tally.hpp"
#include "../src/core/vote_store.hpp"
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

nexusvote::VoteRecord rec(const std::string& office, nexusvote::VoteKind k, const std::string& ref,
                          const std::string& id) {
    nexusvote::VoteRecord r;
    r.record_id = id;
    r.office_id = office;
    r.kind = k;
    r.ref = ref;
    return r;
}

} // namespace

TEST(TallyFormula, SumMatchesInvariant) {
    auto c = cfg();
    std::vector<nexusvote::VoteRecord> v = {
        rec("vereador", nexusvote::VoteKind::Candidate, "c-ada",
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        rec("vereador", nexusvote::VoteKind::Candidate, "c-ada",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
        rec("vereador", nexusvote::VoteKind::Party, "11", "cccccccccccccccccccccccccccccccc"),
        rec("vereador", nexusvote::VoteKind::Blank, "", "dddddddddddddddddddddddddddddddd"),
        rec("vereador", nexusvote::VoteKind::Null, "", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"),
    };
    auto t = nexusvote::tallyVotes(c, v);
    EXPECT_TRUE(t.ok);
    const auto& o = t.tally.by_office.at("vereador");
    EXPECT_EQ(o.nominal.at("c-ada"), 2u);
    EXPECT_EQ(o.legend.at("11"), 1u);
    EXPECT_EQ(o.blank, 1u);
    EXPECT_EQ(o.null, 1u);
    EXPECT_EQ(o.total, 5u);
}

TEST(TallyFormula, OfficesNeverMixed) {
    auto c = cfg();
    std::vector<nexusvote::VoteRecord> v = {
        rec("vereador", nexusvote::VoteKind::Candidate, "c-ada",
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        rec("alcaide", nexusvote::VoteKind::Candidate, "c-duarte",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
    };
    auto t = nexusvote::tallyVotes(c, v);
    EXPECT_TRUE(t.ok);
    EXPECT_EQ(t.tally.by_office.at("vereador").total, 1u);
    EXPECT_EQ(t.tally.by_office.at("alcaide").total, 1u);
}

TEST(TallyFormula, UnknownOfficeFails) {
    auto c = cfg();
    std::vector<nexusvote::VoteRecord> v = {
        rec("fantasma", nexusvote::VoteKind::Null, "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
    };
    EXPECT_FALSE(nexusvote::tallyVotes(c, v).ok);
}

TEST(StoreRoundtrip, SerializeDeserialize) {
    nexusvote::VoteRecord r = rec("alcaide", nexusvote::VoteKind::Candidate, "c-elisa",
                                  "abcdef0123456789abcdef0123456789");
    std::string line = nexusvote::VoteStore::serialize(r);
    nexusvote::VoteRecord back;
    std::string err;
    EXPECT_TRUE(nexusvote::VoteStore::deserialize(line, back, err));
    EXPECT_EQ(back.record_id, r.record_id);
    EXPECT_EQ(back.ref, "c-elisa");
}

TEST(StoreRoundtrip, RejectsCorruptLine) {
    nexusvote::VoteRecord back;
    std::string err;
    EXPECT_FALSE(nexusvote::VoteStore::deserialize("{truncado", back, err));
    EXPECT_FALSE(
        nexusvote::VoteStore::deserialize("{\"schema_version\":1,\"record_id\":\"zzz\",\"office_"
                                          "id\":\"a\",\"kind\":\"null\",\"ref\":\"\"}",
                                          back, err));
}

TEST(Recovery, PartialSuffixDiscarded) {
    std::string dir = std::string(TMP_BASE) + "/partialsuf";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    std::string jp = dir + "/j.jsonl";
    {
        std::ofstream f(jp, std::ios::binary);
        f << nexusvote::VoteStore::serialize(
                 rec("alcaide", nexusvote::VoteKind::Blank, "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))
          << "\n{\"schema_version\":1,\"record_id\":\"incompleto";
    }
    nexusvote::VoteStore st(jp);
    auto r = st.open();
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.recovered_partial);
    EXPECT_EQ(st.size(), 1u);
}

TEST(Recovery, CorruptCopyDetected) {
    std::string dir = std::string(TMP_BASE) + "/corrupt";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    std::string jp = dir + "/j.jsonl";
    {
        std::ofstream f(jp, std::ios::binary);
        f << "lixo total\n";
    }
    nexusvote::VoteStore st(jp);
    EXPECT_FALSE(st.open().ok);
}
