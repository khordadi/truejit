#include "BranchProfiler.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void BranchProfiler::update(const Event &Event) {
  if (const auto [Line, Cond] = Event; Cond)
    Branches[Line].True++;
  else
    Branches[Line].False++;
}

BranchProfiler::~BranchProfiler() {
  json J;
  for (const auto &[Line, Branch] : Branches)
    J[std::to_string(Line)] = {Branch.True, Branch.False};

  std::ofstream O("/tmp/branches.json");
  O << J.dump(2);
  O.close();
}
