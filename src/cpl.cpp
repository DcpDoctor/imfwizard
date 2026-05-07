#include "imfwizard/cpl.h"
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace imfwizard
{

static const char* CPL_NS = "http://www.smpte-ra.org/schemas/2067-3/2016";
static const char* CORE_NS = "http://www.smpte-ra.org/schemas/2067-2/2016";

CplResult generate_cpl(const CplOptions& opts, const std::filesystem::path& output_dir)
{
  std::string cpl_uuid = generate_uuid_bare();
  std::filesystem::path cpl_path = output_dir / ("CPL_" + cpl_uuid + ".xml");

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "CompositionPlaylist");
  xmlDocSetRootElement(doc, root);
  xmlNewNs(root, BAD_CAST CPL_NS, nullptr);
  xmlNsPtr core_ns = xmlNewNs(root, BAD_CAST CORE_NS, BAD_CAST "cc");

  xmlNewChild(root, nullptr, BAD_CAST "Id", BAD_CAST("urn:uuid:" + cpl_uuid).c_str());
  xmlNewChild(root, nullptr, BAD_CAST "ContentTitle", BAD_CAST opts.title.c_str());
  xmlNewChild(root, nullptr, BAD_CAST "Issuer", BAD_CAST opts.issuer.c_str());
  xmlNewChild(root, nullptr, BAD_CAST "ContentKind", BAD_CAST opts.content_kind.c_str());

  std::string edit_rate_str =
      std::to_string(opts.edit_rate_num) + " " + std::to_string(opts.edit_rate_den);
  xmlNewChild(root, nullptr, BAD_CAST "EditRate", BAD_CAST edit_rate_str.c_str());

  // SegmentList
  xmlNodePtr seg_list = xmlNewChild(root, nullptr, BAD_CAST "SegmentList", nullptr);
  xmlNodePtr segment = xmlNewChild(seg_list, nullptr, BAD_CAST "Segment", nullptr);
  xmlNewChild(segment, nullptr, BAD_CAST "Id", BAD_CAST generate_uuid().c_str());

  // SequenceList
  xmlNodePtr seq_list = xmlNewChild(segment, nullptr, BAD_CAST "SequenceList", nullptr);

  // Video sequence
  if(!opts.video_tracks.empty())
  {
    xmlNodePtr video_seq = xmlNewChild(seq_list, nullptr, BAD_CAST "cc:MainImageSequence", nullptr);
    xmlNewChild(video_seq, core_ns, BAD_CAST "Id", BAD_CAST generate_uuid().c_str());
    xmlNewChild(video_seq, core_ns, BAD_CAST "TrackId", BAD_CAST generate_uuid().c_str());

    xmlNodePtr res_list = xmlNewChild(video_seq, core_ns, BAD_CAST "ResourceList", nullptr);
    for(const auto& track : opts.video_tracks)
    {
      xmlNodePtr resource = xmlNewChild(res_list, core_ns, BAD_CAST "Resource", nullptr);
      xmlNewChild(resource, core_ns, BAD_CAST "Id", BAD_CAST generate_uuid().c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "TrackFileId",
                  BAD_CAST("urn:uuid:" + track.uuid).c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "EditRate", BAD_CAST edit_rate_str.c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "IntrinsicDuration",
                  BAD_CAST std::to_string(track.duration).c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "EntryPoint", BAD_CAST "0");
      xmlNewChild(resource, core_ns, BAD_CAST "SourceDuration",
                  BAD_CAST std::to_string(track.duration).c_str());
    }
  }

  // Audio sequence
  if(!opts.audio_tracks.empty())
  {
    xmlNodePtr audio_seq = xmlNewChild(seq_list, nullptr, BAD_CAST "cc:MainAudioSequence", nullptr);
    xmlNewChild(audio_seq, core_ns, BAD_CAST "Id", BAD_CAST generate_uuid().c_str());
    xmlNewChild(audio_seq, core_ns, BAD_CAST "TrackId", BAD_CAST generate_uuid().c_str());

    xmlNodePtr res_list = xmlNewChild(audio_seq, core_ns, BAD_CAST "ResourceList", nullptr);
    for(const auto& track : opts.audio_tracks)
    {
      xmlNodePtr resource = xmlNewChild(res_list, core_ns, BAD_CAST "Resource", nullptr);
      xmlNewChild(resource, core_ns, BAD_CAST "Id", BAD_CAST generate_uuid().c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "TrackFileId",
                  BAD_CAST("urn:uuid:" + track.uuid).c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "EditRate", BAD_CAST edit_rate_str.c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "IntrinsicDuration",
                  BAD_CAST std::to_string(track.duration).c_str());
      xmlNewChild(resource, core_ns, BAD_CAST "EntryPoint", BAD_CAST "0");
      xmlNewChild(resource, core_ns, BAD_CAST "SourceDuration",
                  BAD_CAST std::to_string(track.duration).c_str());
    }
  }

  xmlSaveFormatFileEnc(cpl_path.string().c_str(), doc, "UTF-8", 1);
  xmlFreeDoc(doc);

  CplResult result;
  result.uuid = cpl_uuid;
  result.path = cpl_path;
  result.hash = sha1_base64(cpl_path);
  result.size = std::filesystem::file_size(cpl_path);

  spdlog::info("Generated CPL: {}", cpl_path.string());
  return result;
}

} // namespace imfwizard
