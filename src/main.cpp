#include "imfwizard/imfwizard.h"
#include "imfwizard/info.h"
#include "imfwizard/supplemental.h"
#include "imfwizard/validate.h"
#include "imfwizard/transcode.h"
#include "imfwizard/dolby_vision.h"
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

    // === ENCODE subcommand ===
    auto* encode_cmd = app.add_subcommand("encode", "Encode image sequence to JPEG 2000");
    std::string enc_input, enc_output;
    float enc_bitrate = 250.0f;
    uint32_t enc_threads = 0;
    bool enc_cinema = true;

    encode_cmd->add_option("-i,--input", enc_input, "Input image sequence directory")->required()
        ->check(CLI::ExistingDirectory);
    encode_cmd->add_option("-o,--output", enc_output, "Output J2K directory")->required();
    encode_cmd->add_option("--bitrate", enc_bitrate, "Target bitrate (Mbps)")->default_val(250.0f);
    encode_cmd->add_option("--threads", enc_threads, "Number of threads (0=auto)")->default_val(0);
    encode_cmd->add_flag("--no-cinema", [&](int64_t){ enc_cinema = false; }, "Disable cinema profile");

    // === VALIDATE subcommand ===
    auto* val_cmd = app.add_subcommand("validate", "Validate an existing IMP (via Photon)");
    std::string val_dir;
    val_cmd->add_option("imp_dir", val_dir, "IMP directory to validate")->required()
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

    // === TRANSCODE subcommand ===
    auto* trans_cmd = app.add_subcommand("transcode", "Transcode video file to image sequence");
    std::string trans_input, trans_output, trans_format = "tiff";
    uint16_t trans_bit_depth = 16;
    uint32_t trans_threads = 0;

    trans_cmd->add_option("-i,--input", trans_input, "Input video file (ProRes, DNxHR, etc.)")->required()
        ->check(CLI::ExistingFile);
    trans_cmd->add_option("-o,--output", trans_output, "Output image sequence directory")->required();
    trans_cmd->add_option("-f,--format", trans_format, "Output format (tiff, dpx, exr, png)")->default_val("tiff");
    trans_cmd->add_option("--bit-depth", trans_bit_depth, "Output bit depth")->default_val(16);
    trans_cmd->add_option("--threads", trans_threads, "Number of threads (0=auto)")->default_val(0);

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

    if (encode_cmd->parsed()) {
        imfwizard::EncodeOptions opts;
        opts.input_dir = enc_input;
        opts.output_dir = enc_output;
        opts.target_bitrate_mbps = enc_bitrate;
        opts.num_threads = enc_threads;
        opts.cinema_profile = enc_cinema;

        auto result = imfwizard::encode_to_j2k(opts);
        if (!result.success) {
            spdlog::error("Encoding failed: {}", result.error);
            return 1;
        }

        std::cout << "Encoded " << result.frame_count << " frames to "
                  << result.output_dir << "\n";
        return 0;
    }

    if (val_cmd->parsed()) {
        auto result = imfwizard::validate_with_photon(val_dir);
        if (result.valid) {
            std::cout << "IMP is valid.\n";
        } else {
            std::cout << "IMP validation issues:\n";
        }
        for (const auto& note : result.notes) {
            const char* sev = "INFO";
            if (note.severity == imfwizard::ValidationNote::Severity::error) sev = "ERROR";
            else if (note.severity == imfwizard::ValidationNote::Severity::warning) sev = "WARN";
            std::cout << "  [" << sev << "] " << note.message << "\n";
        }
        return result.valid ? 0 : 1;
    }

    if (trans_cmd->parsed()) {
        imfwizard::TranscodeOptions opts;
        opts.input_file = trans_input;
        opts.output_dir = trans_output;
        opts.output_format = trans_format;
        opts.bit_depth = trans_bit_depth;
        opts.threads = trans_threads;

        auto result = imfwizard::transcode_to_sequence(opts);
        if (!result.success) {
            spdlog::error("Transcode failed: {}", result.error);
            return 1;
        }

        std::cout << "Transcoded " << result.frame_count << " frames ("
                  << result.width << "x" << result.height << " @ "
                  << result.fps << " fps) to " << result.output_dir << "\n";
        return 0;
    }

    return 0;
}
