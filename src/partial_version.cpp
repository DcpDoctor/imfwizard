#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

#include "imfwizard/partial_version.h"
#include "imfwizard/info.h"
#include "imfwizard/uuid.h"

namespace fs = std::filesystem;

namespace imfwizard
{

PartialVersionResult create_partial_version(const PartialVersionOptions& opts)
{
  PartialVersionResult result;

  if(!fs::exists(opts.ov_imp_dir))
  {
    result.error = "Original Version IMP not found: " + opts.ov_imp_dir.string();
    return result;
  }
  if(!fs::exists(opts.replacement_video))
  {
    result.error = "Replacement video not found: " + opts.replacement_video.string();
    return result;
  }

  auto ov_info = read_imp_info(opts.ov_imp_dir);
  if(!ov_info.valid)
  {
    result.error = "Cannot read OV IMP: " + ov_info.error;
    return result;
  }

  fs::create_directories(opts.output_dir);

  // Copy replacement video to output
  auto video_filename = opts.replacement_video.filename();
  fs::copy_file(opts.replacement_video, opts.output_dir / video_filename,
                fs::copy_options::overwrite_existing);

  // Copy replacement audio if provided
  if(!opts.replacement_audio.empty() && fs::exists(opts.replacement_audio))
  {
    auto audio_filename = opts.replacement_audio.filename();
    fs::copy_file(opts.replacement_audio, opts.output_dir / audio_filename,
                  fs::copy_options::overwrite_existing);
  }

  // Generate supplemental CPL referencing OV and replacement segments
  result.cpl_uuid = generate_uuid();
  auto cpl_path = opts.output_dir / "CPL.xml";

  std::ofstream cpl(cpl_path);
  cpl << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  cpl << "<CompositionPlaylist xmlns=\"http://www.smpte-ra.org/schemas/2067-3/2016\">\n";
  cpl << "  <Id>urn:uuid:" << result.cpl_uuid << "</Id>\n";
  cpl << "  <ContentTitle>" << opts.title << "</ContentTitle>\n";
  cpl << "  <Issuer>" << opts.issuer << "</Issuer>\n";
  cpl << "  <ContentKind>feature</ContentKind>\n";
  cpl << "  <!-- Supplemental: references OV " << ov_info.cpl_uuid << " -->\n";
  cpl << "  <SegmentList>\n";

  for(auto& seg : opts.segments)
  {
    cpl << "    <Segment>\n";
    cpl << "      <!-- Replaces reel " << seg.reel_index << " frames " << seg.start_frame << "-"
        << seg.end_frame << " -->\n";
    cpl << "      <SequenceList>\n";
    cpl << "        <MainImageSequence>\n";
    cpl << "          <TrackFileId>urn:uuid:" << generate_uuid() << "</TrackFileId>\n";
    cpl << "          <FileName>" << video_filename.string() << "</FileName>\n";
    cpl << "          <EntryPoint>" << seg.start_frame << "</EntryPoint>\n";
    if(seg.end_frame > seg.start_frame)
      cpl << "          <Duration>" << (seg.end_frame - seg.start_frame) << "</Duration>\n";
    cpl << "        </MainImageSequence>\n";
    cpl << "      </SequenceList>\n";
    cpl << "    </Segment>\n";
    result.segments_replaced++;
  }

  cpl << "  </SegmentList>\n";
  cpl << "</CompositionPlaylist>\n";
  cpl.close();

  result.output_dir = opts.output_dir;
  result.success = true;
  return result;
}

} // namespace imfwizard
