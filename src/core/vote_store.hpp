// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Journal de votos de laboratório: append-only, flush+fsync por confirmação,

#include <cstdint>
#include <string>
#include <vector>

#include "domain.hpp"

namespace nexusvote {

struct StoreResult {
    bool ok = false;
    std::string error;
    bool recovered_partial = false;
};

class VoteStore {
  public:
    explicit VoteStore(const std::string& journal_path) : path_(journal_path) {}

    StoreResult open();
    StoreResult append(const VoteRecord& rec);
    const std::vector<VoteRecord>& all() const {
        return records_;
    }
    std::size_t size() const {
        return records_.size();
    }

    static std::string serialize(const VoteRecord& rec);
    static bool deserialize(const std::string& line, VoteRecord& rec, std::string& err);

  private:
    std::string path_;
    std::vector<VoteRecord> records_;
};

} // namespace nexusvote
