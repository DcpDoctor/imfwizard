#pragma once

#include "imfwizard/validate.h"
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

// Generate a QC report in HTML format
struct QcReportOptions
{
  std::filesystem::path imp_dir;
  ValidationResult validation;
  std::string title;
  std::string author = "IMF Wizard";
};

// Generate HTML QC report
std::string generate_qc_report_html(const QcReportOptions& opts);

// Write QC report to file
void write_qc_report(const QcReportOptions& opts, const std::filesystem::path& output_path);

} // namespace imfwizard
