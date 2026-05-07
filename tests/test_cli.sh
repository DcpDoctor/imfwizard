#!/bin/bash
# CLI integration tests for imfwizard
# Run from the build directory: bash ../tests/test_cli.sh

set -e

BINARY="./imfwizard"
PASS=0
FAIL=0

pass() { echo "  PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1 — $2"; FAIL=$((FAIL + 1)); }

check_exit() {
  local desc="$1"; shift
  if "$@" > /dev/null 2>&1; then
    pass "$desc"
  else
    fail "$desc" "exit code $?"
  fi
}

check_exit_fail() {
  local desc="$1"; shift
  if "$@" > /dev/null 2>&1; then
    fail "$desc" "expected non-zero exit"
  else
    pass "$desc"
  fi
}

check_output_contains() {
  local desc="$1"; local needle="$2"; shift 2
  local out
  out=$("$@" 2>&1) || true
  if echo "$out" | grep -qi "$needle"; then
    pass "$desc"
  else
    fail "$desc" "output missing '$needle'"
  fi
}

echo "IMF Wizard CLI Tests"
echo "===================="
echo

# --- Help & version ---
echo "Help/Version:"
check_exit "help flag" $BINARY --help
check_exit "version flag" $BINARY --version
check_output_contains "version contains 0.1" "0.1" $BINARY --version

# --- Subcommand help ---
echo
echo "Subcommand help:"
for cmd in create info encode validate transcode extract conform watch report loudness qc channel-map upload captions watermark profiles batch daemon; do
  check_output_contains "$cmd --help" "Usage\|usage\|USAGE\|Options\|options\|OPTIONS" $BINARY $cmd --help
done

# --- Create requires arguments ---
echo
echo "Create validation:"
check_exit_fail "create no args" $BINARY create
check_exit_fail "create missing output" $BINARY create --title "Test" --video /nonexistent

# --- Info requires directory ---
echo
echo "Info validation:"
check_exit_fail "info no args" $BINARY info
check_exit_fail "info nonexistent dir" $BINARY info /nonexistent/dir

# --- Encode requires input ---
echo
echo "Encode validation:"
check_exit_fail "encode no args" $BINARY encode
check_exit_fail "encode missing input" $BINARY encode --output /tmp/out

# --- Validate requires directory ---
echo
echo "Validate validation:"
check_exit_fail "validate no args" $BINARY validate
check_exit_fail "validate nonexistent" $BINARY validate /nonexistent

# --- Transcode requires input ---
echo
echo "Transcode validation:"
check_exit_fail "transcode no args" $BINARY transcode
check_exit_fail "transcode missing input" $BINARY transcode -o /tmp

# --- Profiles ---
echo
echo "Profiles:"
check_exit "profiles list" $BINARY profiles
check_output_contains "profiles lists netflix" "netflix" $BINARY profiles

# --- Batch commands (daemon offline) ---
echo
echo "Batch (daemon offline):"
rm -f /tmp/imfwizard.sock
check_exit_fail "batch list no daemon" $BINARY batch list
check_exit_fail "batch status no daemon" $BINARY batch status 1
check_exit_fail "batch cancel no daemon" $BINARY batch cancel 1

# --- Channel map ---
echo
echo "Channel map:"
check_exit "channel-map help" $BINARY channel-map --help

# --- Captions ---
echo
echo "Captions:"
check_exit_fail "captions no args" $BINARY captions
check_exit_fail "captions missing file" $BINARY captions /nonexistent.srt

# --- Report ---
echo
echo "Report:"
check_exit_fail "report no args" $BINARY report
check_exit_fail "report missing dir" $BINARY report /nonexistent

# --- Summary ---
echo
echo "================================"
TOTAL=$((PASS + FAIL))
echo "$PASS/$TOTAL tests passed"
if [ $FAIL -gt 0 ]; then
  echo "$FAIL test(s) FAILED"
  exit 1
fi
echo "All CLI tests passed!"
exit 0
