#include "imfwizard/cpl_annotation.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace imfwizard
{

static std::string iso_timestamp()
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf;
#ifdef _WIN32
  gmtime_s(&tm_buf, &time_t);
#else
  gmtime_r(&time_t, &tm_buf);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

bool annotate_cpl(const std::string& cpl_path, const CplAnnotation& annotation)
{
  std::ifstream in(cpl_path);
  if(!in.is_open())
    return false;

  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();

  // Build annotation XML element
  std::string ann_xml = "  <Annotation>\n"
                        "    <Text>" +
                        annotation.text +
                        "</Text>\n"
                        "    <Author>" +
                        annotation.author +
                        "</Author>\n"
                        "    <Timestamp>" +
                        (annotation.timestamp.empty() ? iso_timestamp() : annotation.timestamp) +
                        "</Timestamp>\n";
  if(!annotation.revision.empty())
    ann_xml += "    <Revision>" + annotation.revision + "</Revision>\n";
  ann_xml += "  </Annotation>\n";

  // Insert before </CompositionPlaylist>
  auto close_pos = content.find("</CompositionPlaylist>");
  if(close_pos == std::string::npos)
    return false;

  content.insert(close_pos, ann_xml);

  std::ofstream out(cpl_path);
  out << content;
  return true;
}

CplVersionInfo read_cpl_annotations(const std::string& cpl_path)
{
  CplVersionInfo info;

  std::ifstream in(cpl_path);
  if(!in.is_open())
    return info;

  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  // Extract UUID
  std::regex uuid_re(R"(<Id>urn:uuid:([^<]+)</Id>)");
  std::smatch uuid_match;
  if(std::regex_search(content, uuid_match, uuid_re))
    info.cpl_uuid = uuid_match[1].str();

  // Extract title
  std::regex title_re(R"(<ContentTitle>([^<]+)</ContentTitle>)");
  std::smatch title_match;
  if(std::regex_search(content, title_match, title_re))
    info.title = title_match[1].str();

  // Extract annotations
  std::regex ann_re(R"(<Annotation>\s*<Text>([^<]*)</Text>\s*<Author>([^<]*)</Author>\s*<Timestamp>([^<]*)</Timestamp>(?:\s*<Revision>([^<]*)</Revision>)?\s*</Annotation>)");
  auto it = std::sregex_iterator(content.begin(), content.end(), ann_re);
  for(; it != std::sregex_iterator(); ++it)
  {
    CplAnnotation ann;
    ann.text = (*it)[1].str();
    ann.author = (*it)[2].str();
    ann.timestamp = (*it)[3].str();
    if((*it)[4].matched)
      ann.revision = (*it)[4].str();
    info.annotations.push_back(ann);
  }

  if(!info.annotations.empty())
    info.current_revision = info.annotations.back().revision;

  return info;
}

bool set_cpl_revision(const std::string& cpl_path, const std::string& revision)
{
  CplAnnotation ann;
  ann.text = "Revision set to " + revision;
  ann.author = "IMF Wizard";
  ann.revision = revision;
  return annotate_cpl(cpl_path, ann);
}

} // namespace imfwizard
