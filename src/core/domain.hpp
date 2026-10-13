// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Protótipo independente de laboratório. NÃO OFICIAL.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nexusvote {

inline constexpr int kSchemaVersion = 1;
inline constexpr const char* kRulesProfile = "LAB-BR-EDU-v1";
#ifndef NEXUSVOTE_VERSION
#define NEXUSVOTE_VERSION "0.0.0-unknown"
#endif
inline constexpr const char* kSoftwareVersion = NEXUSVOTE_VERSION;
inline constexpr const char* kNonOfficialBanner =
    "PROTÓTIPO INDEPENDENTE — USO EXCLUSIVO EM LABORATÓRIO — NÃO OFICIAL";

enum class VoteKind { Candidate, Party, Blank, Null };

struct Party {
    std::string number;
    std::string name;
};

struct Candidate {
    std::string id;
    std::string office_id;
    std::string number;
    std::string name;
    std::string party_number;
    bool eligible = true;
    std::string running_mate;
    std::vector<std::string> substitutes;
};

struct Office {
    std::string id;
    std::string title;
    int number_length = 2;
    bool proportional = false;
    int order = 0;
    int seats = 1;
    bool requires_mate = false;
};

struct ElectionConfig {
    int schema_version = kSchemaVersion;
    std::string election_id;
    std::string display_name;
    std::string locale = "pt-BR";
    std::string rules_profile = kRulesProfile;
    int turn = 1;
    bool legend_enabled = false;
    std::vector<Office> offices;
    std::vector<Party> parties;
    std::vector<Candidate> candidates;
    int test_max_voters = 0;
    std::string test_until_iso;
    std::uint64_t electorate_size = 0;
};

struct ElectionRules {
    int version = 1;
    bool legend_enabled = false;
};

enum class PreviewType { Nominal, Legend, Blank, Null, Incomplete, Invalid };

struct BallotPreview {
    PreviewType type = PreviewType::Invalid;
    std::string office_id;
    std::string digits;
    std::string candidate_id;
    std::string party_number;
    std::string message_pt;
};

struct VoteRecord {
    std::string record_id;
    std::string office_id;
    VoteKind kind = VoteKind::Null;
    std::string ref;
};

struct OfficeTally {
    std::map<std::string, std::uint64_t> nominal;
    std::map<std::string, std::uint64_t> legend;
    std::uint64_t blank = 0;
    std::uint64_t null = 0;
    std::uint64_t total = 0;
};

struct SessionTally {
    std::map<std::string, OfficeTally> by_office;
};

std::string voteKindToString(VoteKind k);
std::string previewTypeToString(PreviewType t);

} // namespace nexusvote
