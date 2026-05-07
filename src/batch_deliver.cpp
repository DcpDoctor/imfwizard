#include <spdlog/spdlog.h>
#include <filesystem>

#include "imfwizard/batch_deliver.h"
#include "imfwizard/profiles.h"
#include "imfwizard/imfwizard.h"
#include "imfwizard/encode.h"

namespace imfwizard
{

const std::vector<std::string> available_delivery_targets = {
    "netflix", "disney", "amazon", "apple", "cinema", "broadcast", "archival"};

BatchDeliverResult batch_deliver(const BatchDeliverOptions& opts)
{
  BatchDeliverResult result;
  result.succeeded = 0;
  result.failed = 0;

  if(opts.targets.empty())
  {
    result.failed = 1;
    BatchDeliverResult::TargetResult tr;
    tr.profile = "(none)";
    tr.success = false;
    tr.error = "No delivery targets specified";
    result.results.push_back(tr);
    return result;
  }

  for(const auto& target : opts.targets)
  {
    BatchDeliverResult::TargetResult tr;
    tr.profile = target.profile;
    tr.output_dir = target.output_dir;

    spdlog::info("Creating delivery for profile: {}", target.profile);

    try
    {
      auto profile = get_profile(target.profile);

      // Build IMP options from profile constraints
      ImpOptions imp_opts;
      imp_opts.title = opts.title;
      imp_opts.issuer = opts.issuer;
      imp_opts.video_dir = opts.video_dir.string();
      imp_opts.audio_file = opts.audio_file.string();
      imp_opts.subtitle_file = opts.subtitle_file.string();
      imp_opts.output_dir = target.output_dir.string();
      imp_opts.edit_rate_num = opts.fps_num;
      imp_opts.edit_rate_den = opts.fps_den;
      imp_opts.content_kind = "feature";

      auto imp_result = create_ov_imp(imp_opts);
      if(!imp_result.success)
      {
        tr.success = false;
        tr.error = imp_result.error;
        result.failed++;
      }
      else
      {
        tr.success = true;
        tr.output_dir = imp_result.output_dir;
        result.succeeded++;
      }
    }
    catch(const std::exception& e)
    {
      tr.success = false;
      tr.error = e.what();
      result.failed++;
    }

    result.results.push_back(tr);
  }

  result.all_success = (result.failed == 0);
  return result;
}

} // namespace imfwizard
