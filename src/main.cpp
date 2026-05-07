#include "imfwizard/imfwizard.h"
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
    std::string video_dir, audio_file, output_dir;
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
    create_cmd->add_flag("--verbose", verbose, "Verbose output");

    // === INFO subcommand ===
    auto* info_cmd = app.add_subcommand("info", "Display information about an existing IMP");
    std::string imp_dir;
    info_cmd->add_option("imp_dir", imp_dir, "IMP directory")->required()
        ->check(CLI::ExistingDirectory);

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
        // TODO: Parse existing IMP and display metadata
        spdlog::info("Reading IMP: {}", imp_dir);
        std::cout << "IMP info not yet implemented\n";
        return 0;
    }

    return 0;
}
