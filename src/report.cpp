#include "imfwizard/report.h"
#include "imfwizard/info.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

namespace imfwizard {

std::string generate_qc_report_html(const QcReportOptions& opts)
{
    auto info = read_imp_info(opts.imp_dir);
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", localtime(&time_t_now));

    int errors = 0, warnings = 0;
    for (const auto& n : opts.validation.notes) {
        if (n.severity == ValidationNote::Severity::error) ++errors;
        else if (n.severity == ValidationNote::Severity::warning) ++warnings;
    }

    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>QC Report — )" << opts.title << R"(</title>
<style>
:root { --bg: #0d1117; --card: #161b22; --border: #30363d; --text: #c9d1d9; --accent: #a78bfa; --red: #f85149; --yellow: #d29922; --green: #3fb950; }
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); padding: 40px; line-height: 1.6; }
.container { max-width: 900px; margin: 0 auto; }
h1 { color: #fff; margin-bottom: 8px; }
.meta { color: #8b949e; margin-bottom: 32px; }
.summary { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 16px; margin-bottom: 32px; }
.stat { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 20px; text-align: center; }
.stat .value { font-size: 2rem; font-weight: 700; color: #fff; }
.stat .label { font-size: 0.85rem; color: #8b949e; }
.stat.pass .value { color: var(--green); }
.stat.fail .value { color: var(--red); }
.stat.warn .value { color: var(--yellow); }
table { width: 100%; border-collapse: collapse; margin-bottom: 32px; }
th, td { padding: 12px 16px; text-align: left; border-bottom: 1px solid var(--border); }
th { background: var(--card); color: #fff; font-weight: 600; }
.badge { display: inline-block; padding: 2px 8px; border-radius: 4px; font-size: 0.75rem; font-weight: 600; }
.badge-error { background: rgba(248,81,73,0.15); color: var(--red); }
.badge-warn { background: rgba(210,153,34,0.15); color: var(--yellow); }
.badge-info { background: rgba(167,139,250,0.15); color: var(--accent); }
.badge-pass { background: rgba(63,185,80,0.15); color: var(--green); }
</style>
</head>
<body>
<div class="container">
<h1>QC Report</h1>
<p class="meta">)" << opts.title << " — Generated " << date_buf << " by " << opts.author << R"(</p>

<div class="summary">
<div class="stat )" << (opts.validation.valid ? "pass" : "fail") << R"(">
<div class="value">)" << (opts.validation.valid ? "PASS" : "FAIL") << R"(</div>
<div class="label">Overall</div>
</div>
<div class="stat )" << (errors > 0 ? "fail" : "pass") << R"(">
<div class="value">)" << errors << R"(</div>
<div class="label">Errors</div>
</div>
<div class="stat )" << (warnings > 0 ? "warn" : "pass") << R"(">
<div class="value">)" << warnings << R"(</div>
<div class="label">Warnings</div>
</div>
<div class="stat">
<div class="value">)" << info.tracks.size() << R"(</div>
<div class="label">Tracks</div>
</div>
</div>
)";

    // IMP info table
    html << R"(<h2 style="color:#fff; margin-bottom:16px">Package Info</h2>
<table>
<tr><th>Property</th><th>Value</th></tr>
<tr><td>Title</td><td>)" << info.cpl_title << R"(</td></tr>
<tr><td>CPL UUID</td><td>)" << info.cpl_uuid << R"(</td></tr>
<tr><td>PKL UUID</td><td>)" << info.pkl_uuid << R"(</td></tr>
<tr><td>Issuer</td><td>)" << info.issuer << R"(</td></tr>
<tr><td>Issue Date</td><td>)" << info.issue_date << R"(</td></tr>
</table>
)";

    // Track list
    html << R"(<h2 style="color:#fff; margin-bottom:16px">Track Files</h2>
<table>
<tr><th>Type</th><th>Filename</th><th>Size</th></tr>
)";
    for (const auto& t : info.tracks) {
        html << "<tr><td>" << t.type << "</td><td>" << t.filename
             << "</td><td>" << t.size << " bytes</td></tr>\n";
    }
    html << "</table>\n";

    // Validation notes
    if (!opts.validation.notes.empty()) {
        html << R"(<h2 style="color:#fff; margin-bottom:16px">Validation Notes</h2>
<table>
<tr><th>Severity</th><th>Message</th></tr>
)";
        for (const auto& note : opts.validation.notes) {
            const char* badge_class = "badge-info";
            const char* label = "INFO";
            if (note.severity == ValidationNote::Severity::error) {
                badge_class = "badge-error"; label = "ERROR";
            } else if (note.severity == ValidationNote::Severity::warning) {
                badge_class = "badge-warn"; label = "WARN";
            }
            html << "<tr><td><span class=\"badge " << badge_class << "\">"
                 << label << "</span></td><td>" << note.message << "</td></tr>\n";
        }
        html << "</table>\n";
    }

    html << "</div>\n</body>\n</html>";
    return html.str();
}

void write_qc_report(const QcReportOptions& opts, const std::filesystem::path& output_path)
{
    auto html = generate_qc_report_html(opts);
    std::ofstream f(output_path);
    f << html;
    spdlog::info("QC report written to {}", output_path.string());
}

} // namespace imfwizard
