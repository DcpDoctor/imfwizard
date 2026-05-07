#pragma once

#include "imfwizard/validate.h"
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct PdfReportOptions
{
  std::filesystem::path imp_dir;
  std::filesystem::path output_path;
  ValidationResult validation;
  std::string title;
  std::string author = "IMF Wizard";
  bool include_asset_hashes = true;
  bool include_timestamps = true;
  bool include_loudness = false;
};

struct PdfReportResult
{
  bool success = false;
  std::filesystem::path output_path;
  uint32_t page_count = 0;
  std::string error;
};

// Generate a PDF QC report for an IMP
// Uses HTML-to-PDF conversion via wkhtmltopdf or weasyprint
PdfReportResult generate_pdf_report(const PdfReportOptions& opts);

// Check if a PDF renderer is available
bool pdf_renderer_available();

} // namespace imfwizard
