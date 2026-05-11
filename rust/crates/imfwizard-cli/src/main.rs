use clap::{Parser, Subcommand};
use std::path::PathBuf;

#[derive(Parser)]
#[command(
    name = "imfwizard",
    version,
    about = "IMF Wizard - Interoperable Master Format creation tool"
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Enable verbose output
    #[arg(short, long, global = true)]
    verbose: bool,
}

#[derive(Subcommand)]
enum Commands {
    /// Create a new IMP (Interoperable Master Package)
    Create {
        /// Output directory for the IMP
        #[arg(short, long)]
        output: PathBuf,

        /// Title of the content
        #[arg(short, long)]
        title: String,

        /// Content kind (feature, trailer, etc.)
        #[arg(short, long, default_value = "feature")]
        kind: String,

        /// Frame rate numerator
        #[arg(long, default_value = "24")]
        fps_num: u32,

        /// Frame rate denominator
        #[arg(long, default_value = "1")]
        fps_den: u32,
    },

    /// Encode image sequence to J2K codestreams
    Encode {
        /// Input directory of image frames
        #[arg(short, long)]
        input: PathBuf,

        /// Output directory for J2K codestreams
        #[arg(short, long)]
        output: PathBuf,

        /// Target bitrate in Mbps
        #[arg(short, long, default_value = "250")]
        bitrate: f64,

        /// Number of threads
        #[arg(short, long, default_value = "0")]
        threads: u32,
    },

    /// Transcode media via ffmpeg
    Transcode {
        /// Input file
        #[arg(short, long)]
        input: PathBuf,

        /// Output file
        #[arg(short, long)]
        output: PathBuf,

        /// Video codec
        #[arg(short, long, default_value = "libx264")]
        codec: String,
    },

    /// Convert subtitles to TTML for IMF
    SubtitleConvert {
        /// Input subtitle file
        #[arg(short, long)]
        input: PathBuf,

        /// Output TTML file
        #[arg(short, long)]
        output: PathBuf,
    },

    /// Extract HDR10+ metadata
    Hdr10plusExtract {
        /// Input HEVC file
        #[arg(short, long)]
        input: PathBuf,

        /// Output JSON file
        #[arg(short, long)]
        output: PathBuf,
    },

    /// Extract Dolby Vision RPU
    DvExtract {
        /// Input HEVC file
        #[arg(short, long)]
        input: PathBuf,

        /// Output RPU file
        #[arg(short, long)]
        output: PathBuf,
    },

    /// Analyze an IMP directory
    Analyze {
        /// IMP directory to analyze
        #[arg(short, long)]
        input: PathBuf,

        /// Output as JSON
        #[arg(long)]
        json: bool,
    },

    /// Compute hash of a file
    Hash {
        /// File to hash
        file: PathBuf,

        /// Hash algorithm (sha1, sha256)
        #[arg(short, long, default_value = "sha1")]
        algorithm: String,
    },

    /// Watch directory for changes
    Watch {
        /// Directory to watch
        dir: PathBuf,
    },

    /// List delivery profiles
    Profiles,

    /// Show timecode conversion
    Timecode {
        /// Timecode string (HH:MM:SS:FF)
        tc: String,

        /// Frame rate
        #[arg(short, long, default_value = "24")]
        fps: u8,
    },
}

fn main() {
    let cli = Cli::parse();

    let level = if cli.verbose { "debug" } else { "info" };
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env().unwrap_or_else(|_| level.into()),
        )
        .init();

    match cli.command {
        Commands::Create {
            output,
            title,
            kind,
            fps_num,
            fps_den,
        } => {
            let opts = imfwizard_core::imp::ImpOptions {
                output_dir: output,
                title,
                content_kind: kind,
                fps_num,
                fps_den,
                ..Default::default()
            };
            let result = imfwizard_core::imp::create_imp(&opts);
            if result.success {
                println!("IMP created at {}", result.output_dir.display());
                println!("  CPL: {}", result.cpl_path.display());
                println!("  PKL: {}", result.pkl_path.display());
                println!("  ASSETMAP: {}", result.assetmap_path.display());
            } else {
                eprintln!("Error: {}", result.error);
                std::process::exit(1);
            }
        }

        Commands::Encode {
            input,
            output,
            bitrate,
            threads,
        } => {
            let opts = imfwizard_core::encode::EncodeOptions {
                input_dir: input,
                output_dir: output,
                bitrate_mbps: bitrate,
                num_threads: threads,
                ..Default::default()
            };
            let result = imfwizard_core::encode::encode(&opts);
            if result.success {
                println!("Encoding complete: {} frames", result.frames_encoded);
            } else {
                eprintln!("Error: {}", result.error);
                std::process::exit(1);
            }
        }

        Commands::Transcode {
            input,
            output,
            codec,
        } => {
            let opts = imfwizard_core::transcode::TranscodeOptions {
                input,
                output,
                codec,
                ..Default::default()
            };
            let result = imfwizard_core::transcode::transcode(&opts);
            if result.success {
                println!("Transcode complete: {}", result.output.display());
            } else {
                eprintln!("Error: {}", result.error);
                std::process::exit(1);
            }
        }

        Commands::SubtitleConvert { input, output } => {
            match imfwizard_core::subtitle_convert::convert_subtitles(
                &input,
                &output,
                imfwizard_core::subtitle_convert::SubtitleFormat::ImscTtml,
            ) {
                Ok(()) => println!("Converted to {}", output.display()),
                Err(e) => {
                    eprintln!("Error: {e}");
                    std::process::exit(1);
                }
            }
        }

        Commands::Hdr10plusExtract { input, output } => {
            match imfwizard_core::hdr::extract_hdr10plus(&input, &output) {
                Ok(meta) => println!("HDR10+ metadata written to {}", meta.json_path.display()),
                Err(e) => {
                    eprintln!("Error: {e}");
                    std::process::exit(1);
                }
            }
        }

        Commands::DvExtract { input, output } => {
            match imfwizard_core::dolby_vision::extract_rpu(&input, &output) {
                Ok(()) => println!("RPU extracted to {}", output.display()),
                Err(e) => {
                    eprintln!("Error: {e}");
                    std::process::exit(1);
                }
            }
        }

        Commands::Analyze { input, json } => match imfwizard_core::analytics::analyze_imp(&input) {
            Ok(a) => {
                if json {
                    println!("{}", serde_json::to_string_pretty(&a).unwrap());
                } else {
                    println!("Total assets: {}", a.total_assets);
                    println!("Video tracks: {}", a.video_tracks);
                    println!("Audio tracks: {}", a.audio_tracks);
                    println!("Subtitle tracks: {}", a.subtitle_tracks);
                    println!("Total size: {} bytes", a.total_size_bytes);
                }
            }
            Err(e) => {
                eprintln!("Error: {e}");
                std::process::exit(1);
            }
        },

        Commands::Hash { file, algorithm } => {
            let algo = match algorithm.as_str() {
                "sha256" => imfwizard_core::hash::HashAlgorithm::Sha256,
                _ => imfwizard_core::hash::HashAlgorithm::Sha1,
            };
            match imfwizard_core::hash::hash_file(&file, algo) {
                Ok(h) => {
                    println!("{} {}", h.hex, file.display());
                }
                Err(e) => {
                    eprintln!("Error: {e}");
                    std::process::exit(1);
                }
            }
        }

        Commands::Watch { dir } => {
            println!("Watching {} for changes...", dir.display());
            match imfwizard_core::watch::FileWatcher::new(&dir) {
                Ok(watcher) => {
                    while let Some(event) = watcher.next_event() {
                        println!("{event:?}");
                    }
                }
                Err(e) => {
                    eprintln!("Error: {e}");
                    std::process::exit(1);
                }
            }
        }

        Commands::Profiles => {
            for p in imfwizard_core::profiles::all_profiles() {
                println!(
                    "{:?}: {}x{} @ {} Mbps, {} fps",
                    p.platform, p.width, p.height, p.bitrate_mbps, p.frame_rate,
                );
            }
        }

        Commands::Timecode { tc, fps } => {
            match imfwizard_core::timecode::Timecode::parse(&tc, fps) {
                Ok(parsed) => {
                    println!("Timecode: {parsed}");
                    println!("Total frames: {}", parsed.to_frames());
                }
                Err(e) => {
                    eprintln!("Error: {e}");
                    std::process::exit(1);
                }
            }
        }
    }
}
