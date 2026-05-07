#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "imfwizard/pdf_report.h"
#include "imfwizard/report.h"
#include "imfwizard/portable.h"

namespace fs = std::filesystem;

namespace imfwizard
{

static bool tool_available(const char* name)
{
#ifdef _WIN32
  std::string cmd = std::string("where ") + name + " >NUL 2>&1";
#else
  std::string cmd = std::string("which ") + name + " >/dev/null 2>&1";
#endif
  return system(cmd.c_str()) == 0;
}

bool pdf_renderer_available()
{
  return tool_available("wkhtmltopdf") || tool_available("weasyprint");
}

PdfReportResult generate_pdf_report(const PdfReportOptions& opts)
{
  PdfReportResult result;

  if(!fs::exists(opts.imp_dir))
  {
    result.error = "IMP directory not found: " + opts.imp_dir.string();
    return result;
  }

  // Generate HTML report first
  QcReportOptions html_opts;
  html_opts.imp_dir = opts.imp_dir;
  html_opts.validation = opts.validation;
  html_opts.title = opts.title.empty() ? opts.imp_dir.filename().string() : opts.title;
  html_opts.author = opts.author;

  std::string html = generate_qc_report_html(html_opts);
  if(html.empty())
  {
    result.error = "Failed to generate HTML report";
    return result;
  }

  // Write HTML to temp file
  auto tmp_html = fs::temp_directory_path() / "imfwizard_report.html";
  {
    std::ofstream f(tmp_html);
    if(!f)
    {
      result.error = "Failed to write temp HTML file";
      return result;
    }
    f << html;
  }

  fs::path output = opts.output_path;
  if(output.empty())
    output = opts.imp_dir / "qc_report.pdf";

  // Try wkhtmltopdf first, then weasyprint
  std::ostringstream cmd;
  if(tool_available("wkhtmltopdf"))
  {
    cmd << "wkhtmltopdf --quiet --page-size A4 --margin-top 15mm --margin-bottom 15mm"
        << " --margin-left 15mm --margin-right 15mm"
        << " --footer-center \"[page] / [topage]\""
        << " --footer-font-size 8"
        << " " << tmp_html.string() << " " << output.string() << " 2>&1";
  }
  else if(tool_available("weasyprint"))
  {
    cmd << "weasyprint " << tmp_html.string() << " " << output.string() << " 2>&1";
  }
  else
  {
    result.error = "No PDF renderer found. Install wkhtmltopdf or weasyprint.";
    fs::remove(tmp_html);
    return result;
  }

  std::array<char, 4096> buf{};
  std::string cmd_output;
  FILE* pipe = portable_popen(cmd.str().c_str(), "r");
  if(!pipe)
  {
    result.error = "Failed to execute PDF renderer";
    fs::remove(tmp_html);
    return result;
  }
  while(fgets(buf.data(), static_cast<int>(buf.size()), pipe))
    cmd_output += buf.data();
  int ret = portable_pclose(pipe);

  fs::remove(tmp_html);

  if(ret != 0)
  {
    result.error = "PDF generation failed: " + cmd_output;
    return result;
  }

  if(!fs::exists(output))
  {
    result.error = "PDF file was not created";
    return result;
  }

  result.success = true;
  result.output_path = output;
  result.page_count = 1; // Approximate — actual count requires parsing the PDF
  spdlog::info("PDF report generated: {}", output.string());

  return result;
}

} // namespace imfwizard
