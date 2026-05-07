#!/usr/bin/env python3
"""CLI integration tests for imfwizard."""

import os
import subprocess
import sys

PASS = 0
FAIL = 0
BINARY = os.path.join(os.path.dirname(__file__), "..", "build", "imfwizard")

if not os.path.isfile(BINARY):
    BINARY = "./imfwizard"


def run(args, expect_fail=False):
    """Run binary with args, return (exit_code, stdout, stderr)."""
    try:
        r = subprocess.run(
            [BINARY] + args,
            capture_output=True, text=True, timeout=10
        )
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "timeout"
    except FileNotFoundError:
        return -1, "", "binary not found"


def check_exit(desc, *args):
    global PASS, FAIL
    code, _, _ = run(list(args))
    if code == 0:
        print(f"  PASS: {desc}")
        PASS += 1
    else:
        print(f"  FAIL: {desc} — exit code {code}")
        FAIL += 1


def check_exit_fail(desc, *args):
    global PASS, FAIL
    code, _, _ = run(list(args))
    if code != 0:
        print(f"  PASS: {desc}")
        PASS += 1
    else:
        print(f"  FAIL: {desc} — expected non-zero exit")
        FAIL += 1


def check_output_contains(desc, needle, *args):
    global PASS, FAIL
    code, stdout, stderr = run(list(args))
    combined = (stdout + stderr).lower()
    if needle.lower() in combined:
        print(f"  PASS: {desc}")
        PASS += 1
    else:
        print(f"  FAIL: {desc} — output missing '{needle}'")
        FAIL += 1


def main():
    global PASS, FAIL

    # Remove any lingering socket to avoid interference
    socket_path = "/tmp/imfwizard.sock"
    if os.path.exists(socket_path):
        os.unlink(socket_path)

    print("IMF Wizard CLI Tests")
    print("====================\n")

    # Help & version
    print("Help/Version:")
    check_exit("help flag", "--help")
    check_exit("version flag", "--version")
    check_output_contains("version contains 0.1", "0.1", "--version")

    # Subcommand help
    print("\nSubcommand help:")
    subcommands = [
        "create", "info", "encode", "validate", "transcode", "extract",
        "conform", "watch", "report", "loudness", "qc", "channel-map",
        "upload", "captions", "watermark", "profiles", "batch", "daemon"
    ]
    for cmd in subcommands:
        check_output_contains(f"{cmd} --help", "options", cmd, "--help")

    # Create requires arguments
    print("\nCreate validation:")
    check_exit_fail("create no args", "create")
    check_exit_fail("create missing output", "create", "--title", "Test", "--video", "/nonexistent")

    # Info requires directory
    print("\nInfo validation:")
    check_exit_fail("info no args", "info")
    check_exit_fail("info nonexistent dir", "info", "/nonexistent/dir")

    # Encode requires input
    print("\nEncode validation:")
    check_exit_fail("encode no args", "encode")
    check_exit_fail("encode missing input", "encode", "--output", "/tmp/out")

    # Validate requires directory
    print("\nValidate validation:")
    check_exit_fail("validate no args", "validate")
    check_exit_fail("validate nonexistent", "validate", "/nonexistent")

    # Transcode requires input
    print("\nTranscode validation:")
    check_exit_fail("transcode no args", "transcode")
    check_exit_fail("transcode missing input", "transcode", "-o", "/tmp")

    # Profiles
    print("\nProfiles:")
    check_exit("profiles list", "profiles")
    check_output_contains("profiles lists netflix", "netflix", "profiles")

    # Batch commands (daemon offline)
    print("\nBatch (daemon offline):")
    # Ensure socket is gone
    if os.path.exists(socket_path):
        os.unlink(socket_path)
    check_exit_fail("batch list no daemon", "batch", "list")
    check_exit_fail("batch status no daemon", "batch", "status", "1")
    check_exit_fail("batch cancel no daemon", "batch", "cancel", "1")

    # Channel map
    print("\nChannel map:")
    check_exit("channel-map help", "channel-map", "--help")

    # Captions
    print("\nCaptions:")
    check_exit_fail("captions no args", "captions")
    check_exit_fail("captions missing file", "captions", "/nonexistent.srt")

    # Report
    print("\nReport:")
    check_exit_fail("report no args", "report")
    check_exit_fail("report missing dir", "report", "/nonexistent")

    # Summary
    print(f"\n{'=' * 32}")
    total = PASS + FAIL
    print(f"{PASS}/{total} tests passed")
    if FAIL > 0:
        print(f"{FAIL} test(s) FAILED")
        return 1
    print("All CLI tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
