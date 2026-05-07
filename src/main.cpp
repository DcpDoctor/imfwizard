#include "imfwizard/imfwizard.h"
#include "imfwizard/info.h"
#include "imfwizard/supplemental.h"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    CLI::App app{"IMF Wizard — Interoperable Master Format package creator"};
    app.set_version_flag("--version", "0.1.0");
    app.require_subcommand(1);

    // === CREATE subcommand ===
    auto* create_cmd = app.add_subcommand("create", "Create an Original Version IMP");

    std::string title, issuer, content_kind = "feature";
    std::string video_dir, audio_file, output_dir, subtitle_file;
    uint32_t fps_num = 24, fps_den = 1;
    uint32_t sample_rate = 48000;
    uint16_t bit_depth = 24;
    bool verbose = false;
    std::string cert_file, key_file;

    create_cmd->add_option("-t,--title", title, "Content title")->required();
    create_cmd->add_option("-i,--issuer", issuer, "Issuer name")->default_val("IMF Wizard");
    create_cmd->add_option("-k,--kind", content_kind, "Content kind")->default_val("feature");
    create_cmd->add_option("-v,--video", video_dir, "Directory of J2K codestreams")->required()
        ->check(CLI::ExistingDirectory);
    create_cmd->add_option("-a,--audio", audio_file, "WAV audio file")
        ->check(CLI::ExistingFile);
    create_cmd->add_option("-o,--output", output_dir, "Output IMP directory")->required();
    create_cmd->add_option("--fps-num", fps_num, "Edit rate numerator")->default_val(24);
    create_cmd->add_option("--fps-den", fps_den, "Edit rate denominator")->default_val(1);
    create_cmd->add_option("--sample-rate", sample_rate, "Audio sample rate")->default_val(48000);
    create_cmd->add_option("--bit-depth", bit_depth, "Audio bit depth")->default_val(24);
    create_cmd->add_option("--cert", cert_file, "X.509 certificate for signing");
    create_cmd->add_option("--key", key_file, "Private key for signing");
    create_cmd->add_option("-s,--subtitle", subtitle_file, "TTML/IMSC subtitle file")
        ->check(CLI::ExistingFile);
    create_cmd->add_flag("--verbose", verbose, "Verbose output");

    // === INFO subcommand ===
    auto* info_cmd = app.add_subcommand("info", "Display information about an existing IMP");
    std::string imp_dir;
    info_cmd->add_option("imp_dir", imp_dir, "IMP directory")->required()
        ->check(CLI::ExistingDirectory);

    // === SUPPLEMENT subcommand ===
    auto* sup_cmd = app.add_subcommand("supplement", "Create a Supplemental IMP");
    std::string sup_title, sup_issuer, sup_ov_dir, sup_video_dir, sup_audio_file, sup_output_dir;
    uint32_t sup_entry_point = 0, sup_duration = 0;

    sup_cmd->add_option("-t,--title", sup_title, "Content title")->required();
    sup_cmd->add_option("-i,--issuer", sup_issuer, "Issuer name")->default_val("IMF Wizard");
    sup_cmd->add_option("--ov", sup_ov_dir, "Original Version IMP directory")->required()
        ->check(CLI::ExistingDirectory);
    sup_cmd->add_option("-v,--video", sup_video_dir, "Replacement J2K directory")
        ->check(CLI::ExistingDirectory);
    sup_cmd->add_option("-a,--audio", sup_audio_file, "Replacement audio WAV")
        ->check(CLI::ExistingFile);
    sup_cmd->add_option("-o,--output", sup_output_dir, "Output supplemental IMP directory")->required();
    sup_cmd->add_option("--entry-point", sup_entry_point, "Frame offset in OV to replace from");
    sup_cmd->add_option("--duration", sup_duration, "Number of frames to replace");

    CLI11_PARSE(app, argc, argv);

    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    if (create_cmd->parsed()) {
        imfwizard::ImpOptions opts;
        opts.title = title;
        opts.issuer = issuer;
        opts.content_kind = content_kind;
        opts.video_dir = video_dir;
        opts.audio_file = audio_file;
        opts.subtitle_file = subtitle_file;
        opts.output_dir = output_dir;
        opts.edit_rate_num = fps_num;
        opts.edit_rate_den = fps_den;
        opts.audio_sample_rate = sample_rate;
        opts.audio_bit_depth = bit_depth;

        if (!cert_file.empty() && !key_file.empty()) {
            imfwizard::SignOptions sign;
            sign.cert_file = cert_file;
            sign.key_file = key_file;
            opts.sign = sign;
        }

        auto result = imfwizard::create_ov_imp(opts);
        if (!result.success) {
            spdlog::error("Failed: {}", result.error);
            return 1;
        }

        std::cout << "IMP created: " << result.output_dir << "\n";
        std::cout << "  CPL: urn:uuid:" << result.cpl_uuid << "\n";
        std::cout << "  PKL: urn:uuid:" << result.pkl_uuid << "\n";
        std::cout << "  Track files: " << result.track_files.size() << "\n";
        return 0;
    }

    if (info_cmd->parsed()) {
        auto imp_info = imfwizard::read_imp_info(imp_dir);
        if (!imp_info.valid) {
            spdlog::error("Failed to read IMP: {}", imp_info.error);
            return 1;
        }

        std::cout << "IMP: " << imp_info.path << "\n";
        std::cout << "  Title: " << imp_info.cpl_title << "\n";
        std::cout << "  CPL: " << imp_info.cpl_uuid << "\n";
        std::cout << "  PKL: " << imp_info.pkl_uuid << "\n";
        std::cout << "  Issuer: " << imp_info.issuer << "\n";
        std::cout << "  Date: " << imp_info.issue_date << "\n";
        std::cout << "  Assets: " << imp_info.tracks.size() << "\n";
        for (const auto& t : imp_info.tracks) {
            std::cout << "    [" << t.type << "] " << t.filename
                      << " (" << t.size << " bytes)\n";
        }
        return 0;
    }

    if (sup_cmd->parsed()) {
        imfwizard::SupplementalOptions opts;
        opts.title = sup_title;
        opts.issuer = sup_issuer;
        opts.ov_imp_dir = sup_ov_dir;
        opts.output_dir = sup_output_dir;
        opts.edit_rate_num = fps_num;
        opts.edit_rate_den = fps_den;

        imfwizard::SupplementalSegment seg;
        seg.entry_point = sup_entry_point;
        seg.duration = sup_duration;
        seg.video_dir = sup_video_dir;
        seg.audio_file = sup_audio_file;
        opts.segments.push_back(seg);

        auto result = imfwizard::create_supplemental_imp(opts);
        if (!result.success) {
            spdlog::error("Failed: {}", result.error);
            return 1;
        }

        std::cout << "Supplemental IMP created: " << result.output_dir << "\n";
        std::cout << "  CPL: urn:uuid:" << result.cpl_uuid << "\n";
        std::cout << "  PKL: urn:uuid:" << result.pkl_uuid << "\n";
        std::cout << "  New track files: " << result.new_track_files.size() << "\n";
        return 0;
    }

    return 0;
}
