#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

enum class ComplianceTarget
{
  Netflix,
  Disney,
  Amazon,
  Apple,
  Cinema2K,
  Cinema4K,
  BroadcastHD,
  BroadcastUHD,
};

struct ComplianceCheck
{
  std::string rule;
  std::string description;
  bool passed = false;
  std::string actual_value;
  std::string expected_value;
};

struct ComplianceOptions
{
  std::filesystem::path imp_dir;
  ComplianceTarget target;
  bool strict = true; // fail on warnings too
};

struct ComplianceResult
{
  ComplianceTarget target;
  std::vector<ComplianceCheck> checks;
  uint32_t passed = 0;
  uint32_t failed = 0;
  uint32_t warnings = 0;
  bool compliant = false;
  bool success = false;
  std::string error;
};

// Run platform-specific compliance checks on an IMP
ComplianceResult check_compliance(const ComplianceOptions& opts);

// Get human-readable target name
std::string compliance_target_name(ComplianceTarget target);

} // namespace imfwizard
