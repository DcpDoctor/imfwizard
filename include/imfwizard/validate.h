#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct ValidationNote
{
  enum class Severity
  {
    error,
    warning,
    info
  };
  Severity severity;
  std::string message;
  std::string context;
};

struct ValidationResult
{
  bool valid = false;
  std::vector<ValidationNote> notes;
  std::string validator_version;
};

// Validation has been moved to dcpdoctor.
// Use: dcpdoctor validate-imp <imp_dir>

} // namespace imfwizard
