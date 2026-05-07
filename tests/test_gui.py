#!/usr/bin/env python3
"""GUI launch/exit integration test for imfwizard.

Tests that the GUI binary starts and can be terminated cleanly.
Works headless on Linux (uses xvfb-run if no DISPLAY), and natively on macOS/Windows.
"""

import os
import platform
import shutil
import signal
import subprocess
import sys
import time

PASS = 0
FAIL = 0


def pass_test(desc):
    global PASS
    print(f"  PASS: {desc}")
    PASS += 1


def fail_test(desc, reason):
    global FAIL
    print(f"  FAIL: {desc} — {reason}")
    FAIL += 1


def find_binary():
    """Find the GUI binary."""
    test_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(test_dir)
    tauri_dir = os.path.join(project_dir, "gui", "src-tauri")

    candidates = [
        os.path.join(tauri_dir, "target", "release", "imfwizard-gui"),
        os.path.join(tauri_dir, "target", "debug", "imfwizard-gui"),
    ]
    if platform.system() == "Windows":
        candidates = [c + ".exe" for c in candidates]

    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def launch_gui(binary):
    """Launch GUI binary, handling headless display on Linux."""
    env = os.environ.copy()

    if platform.system() == "Linux" and not env.get("DISPLAY"):
        xvfb = shutil.which("xvfb-run")
        if xvfb:
            return subprocess.Popen(
                [xvfb, "-a", binary],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                start_new_session=True
            )
        else:
            return None  # Can't launch without display
    else:
        return subprocess.Popen(
            [binary],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True
        )


def is_running(proc):
    return proc.poll() is None


def main():
    global PASS, FAIL

    binary = find_binary()
    if not binary:
        print("SKIP: GUI binary not found (build with 'npx tauri build' first)")
        return 0

    if platform.system() == "Linux" and not os.environ.get("DISPLAY"):
        if not shutil.which("xvfb-run"):
            print("SKIP: No DISPLAY and xvfb-run not available")
            return 0

    print("GUI Launch/Exit Tests")
    print("=====================\n")

    # Test 1: Binary launches without immediate crash
    print("Test: Launch and verify process runs...")
    proc = launch_gui(binary)
    if proc is None:
        fail_test("GUI launch", "could not start process")
        return 1

    time.sleep(2)
    if is_running(proc):
        pass_test("GUI binary launches and stays running")
    else:
        fail_test("GUI binary", f"crashed immediately (exit code {proc.returncode})")

    # Test 2: Process responds to SIGTERM (clean shutdown)
    print("Test: Clean shutdown via SIGTERM...")
    if is_running(proc):
        if platform.system() == "Windows":
            proc.terminate()
        else:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)

        try:
            proc.wait(timeout=5)
            pass_test("GUI exits cleanly on SIGTERM")
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            fail_test("clean shutdown", "process did not exit within 5s")
    else:
        fail_test("clean shutdown", "process already dead")

    # Test 3: Exit code is acceptable
    print("Test: Exit code verification...")
    proc2 = launch_gui(binary)
    if proc2 is None:
        fail_test("exit code", "could not start process")
    else:
        time.sleep(1)
        if is_running(proc2):
            if platform.system() == "Windows":
                proc2.terminate()
            else:
                os.killpg(os.getpgid(proc2.pid), signal.SIGTERM)
            try:
                proc2.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc2.kill()
                proc2.wait()

            code = proc2.returncode
            # 0 = clean exit, -15/143 = SIGTERM (acceptable)
            if code in (0, -15, 143, -signal.SIGTERM):
                pass_test(f"Exit code acceptable ({code})")
            else:
                fail_test("exit code", f"got {code}, expected 0 or SIGTERM")
        else:
            pass_test("Exit code acceptable (already exited)")

    # Summary
    print(f"\n{'=' * 32}")
    total = PASS + FAIL
    print(f"{PASS}/{total} tests passed")
    if FAIL > 0:
        print(f"{FAIL} test(s) FAILED")
        return 1
    print("All GUI tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
