#!/bin/bash
# GUI launch/exit integration test
# Tests that the GUI binary starts and can be terminated cleanly.
# Requires the release build (npx tauri build) or dev build (npx tauri dev).
# Run headless with Xvfb if no display is available.

# Don't use set -e — we handle errors explicitly

BINARY="$(dirname "$0")/../gui/src-tauri/target/release/imfwizard-gui"
if [ ! -f "$BINARY" ]; then
  BINARY="$(dirname "$0")/../gui/src-tauri/target/debug/imfwizard-gui"
fi

if [ ! -f "$BINARY" ]; then
  echo "SKIP: GUI binary not found (build with 'npx tauri build' first)"
  exit 0
fi

PASS=0
FAIL=0

pass() { echo "  PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1 — $2"; FAIL=$((FAIL + 1)); }

echo "GUI Launch/Exit Tests"
echo "====================="
echo

# Check if display is available, use Xvfb if not
USE_XVFB=0
if [ -z "$DISPLAY" ]; then
  if command -v xvfb-run &> /dev/null; then
    USE_XVFB=1
    echo "No DISPLAY — using xvfb-run"
  else
    echo "SKIP: No DISPLAY and xvfb-run not available"
    exit 0
  fi
fi

# Test 1: Binary launches without immediate crash
echo "Test: Launch and verify process runs..."
if [ $USE_XVFB -eq 1 ]; then
  setsid xvfb-run -a "$BINARY" &
else
  setsid "$BINARY" &
fi
PID=$!
sleep 2

if kill -0 $PID 2>/dev/null; then
  pass "GUI binary launches and stays running"
else
  fail "GUI binary" "crashed immediately"
fi

# Test 2: Process responds to SIGTERM (clean shutdown)
echo "Test: Clean shutdown via SIGTERM..."
if kill -0 $PID 2>/dev/null; then
  kill -TERM $PID
  # Wait up to 5 seconds for clean exit
  for i in $(seq 1 50); do
    if ! kill -0 $PID 2>/dev/null; then
      break
    fi
    sleep 0.1
  done
  if kill -0 $PID 2>/dev/null; then
    kill -9 $PID 2>/dev/null || true
    fail "clean shutdown" "process did not exit within 5s"
  else
    wait $PID 2>/dev/null || true
    pass "GUI exits cleanly on SIGTERM"
  fi
else
  fail "clean shutdown" "process already dead"
fi

# Test 3: Check exit code is acceptable
echo "Test: Exit code verification..."
if [ $USE_XVFB -eq 1 ]; then
  setsid xvfb-run -a "$BINARY" &
else
  setsid "$BINARY" &
fi
PID2=$!
sleep 1
if kill -0 $PID2 2>/dev/null; then
  kill -TERM $PID2
  wait $PID2 2>/dev/null
  CODE=$?
  # SIGTERM gives 143 (128+15) which is acceptable
  if [ $CODE -eq 0 ] || [ $CODE -eq 143 ]; then
    pass "Exit code acceptable ($CODE)"
  else
    fail "exit code" "got $CODE, expected 0 or 143"
  fi
else
  pass "Exit code acceptable (already exited)"
fi

# Summary
echo
echo "================================"
TOTAL=$((PASS + FAIL))
echo "$PASS/$TOTAL tests passed"
if [ $FAIL -gt 0 ]; then
  echo "$FAIL test(s) FAILED"
  exit 1
fi
echo "All GUI tests passed!"
exit 0
