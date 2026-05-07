#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct CplAnnotation
{
  std::string author;
  std::string timestamp; // ISO 8601
  std::string text;
  std::string revision; // e.g. "v1.2"
};

struct CplVersionInfo
{
  std::string cpl_uuid;
  std::string title;
  std::vector<CplAnnotation> annotations;
  std::string current_revision;
};

// Add an annotation to a CPL
bool annotate_cpl(const std::string& cpl_path, const CplAnnotation& annotation);

// Read all annotations from a CPL
CplVersionInfo read_cpl_annotations(const std::string& cpl_path);

// Set CPL revision marker
bool set_cpl_revision(const std::string& cpl_path, const std::string& revision);

} // namespace imfwizard
