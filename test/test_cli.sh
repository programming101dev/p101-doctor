#!/usr/bin/env bash
set -euo pipefail

doctor=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-doctor-test.XXXXXX")
allow_file="$PWD/.p101-wrapper-audit-allow"
trap 'rm -rf "$work"; rm -f "$allow_file"' EXIT
fake="$work/fake-tool"

cat >"$fake" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
name=$(basename "$0")
if [ "$name" = p101-observe ]; then
  while [ $# -gt 0 ]; do
    if [ "$1" = -o ]; then
      mkdir -p "$2"
      printf 'resources: fd=0 alloc=0 bad=0\n' >"$2/summary.txt"
      break
    fi
    shift
  done
fi
case "$name" in
  p101-wrapper-audit) status=${P101_DOCTOR_WRAPPER_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  p101-error-contract) status=${P101_DOCTOR_CONTRACT_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  p101-module-map) status=${P101_DOCTOR_MODULE_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  p101-observe) status=${P101_DOCTOR_OBSERVE_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  p101-error-path-walk) status=${P101_DOCTOR_WALK_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  *) status=${P101_DOCTOR_FAKE_STATUS:-0} ;;
esac
exit "$status"
SCRIPT
chmod +x "$fake"
for name in p101-wrapper-audit p101-error-contract p101-module-map p101-run.py p101-observe p101-analyze.py p101-event-model p101-error-path-walk p101-resource-tracker p101-sync-check p101-trace p101-report; do
  ln -s "$fake" "$work/$name"
done

run_expect() {
  expected=$1
  shift
  set +e
  "$doctor" "$@" >/dev/null 2>&1
  actual=$?
  set -e
  [ "$actual" -eq "$expected" ]
}

common=(-A "$work/p101-wrapper-audit" -E "$work/p101-error-contract"
  -M "$work/p101-module-map" -U "$work/p101-run.py" -O "$work/p101-observe"
  -Y "$work/p101-analyze.py" -B "$work/p101-event-model"
  -W "$work/p101-error-path-walk" -r "$work/p101-resource-tracker"
  -d "$work/p101-sync-check" -t "$work/p101-trace" -p "$work/p101-report")

run_expect 0 --help
run_expect 0 -h
touch "$allow_file"
run_expect 0 -v -o "$work/clean" -s src -s include -n 2 -C "$work/compile_commands.json" "${common[@]}" -- /usr/bin/true
rm -f "$allow_file"
P101_DOCTOR_FAKE_STATUS=1 run_expect 1 -o "$work/findings" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_FAKE_STATUS=2 run_expect 2 -o "$work/trouble" "${common[@]}" -- /usr/bin/true
run_expect 0 -x -o "$work/skip" "${common[@]}" -- /usr/bin/true
run_expect 0 -x -C "$work/compile_commands.json" -o "$work/skip-explicit-db" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_OBSERVE_STATUS=2 P101_DOCTOR_WALK_STATUS=2 \
  run_expect 0 -S -o "$work/source-only" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_TEST_OPTION=@ run_expect 2 -x -o "$work/forced-option" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_TEST_OPTION=$'\001' run_expect 2 -x -o "$work/forced-control-option" "${common[@]}" -- /usr/bin/true
for variable in P101_DOCTOR_WRAPPER_STATUS P101_DOCTOR_CONTRACT_STATUS P101_DOCTOR_MODULE_STATUS P101_DOCTOR_OBSERVE_STATUS P101_DOCTOR_WALK_STATUS; do
  env "$variable=1" "$doctor" -o "$work/$variable-findings" "${common[@]}" -- /usr/bin/true >/dev/null 2>&1 || :
  env "$variable=2" "$doctor" -o "$work/$variable-trouble" "${common[@]}" -- /usr/bin/true >/dev/null 2>&1 || :
done

run_expect 2
run_expect 2 -- /usr/bin/true
run_expect 2 -o '' -- /usr/bin/true
run_expect 2 -s '' -- /usr/bin/true
run_expect 2 -n '' -- /usr/bin/true
run_expect 2 -C '' -- /usr/bin/true
for option in A E M O W r d t p; do
  run_expect 2 "-$option" '' -- /usr/bin/true
done
run_expect 2 -n nope -- /usr/bin/true
run_expect 2 -Z -- /usr/bin/true
run_expect 2 "-"$'\001' -- /usr/bin/true
run_expect 2 -o

many_sources=()
for index in $(seq 1 33); do
  many_sources+=(-s "source-$index")
done
run_expect 2 "${many_sources[@]}" -- /usr/bin/true

# Walk every wrapper failure point in the doctor itself. This exercises the
# error paths that ordinary successful fake tools cannot reach.
for index in $(seq 1 180); do
  P101_FAULT_CALL=$index P101_FAULT_ERRNO=5 \
    "$doctor" -o "$work/fault-$index" "${common[@]}" -- /usr/bin/true \
    >/dev/null 2>&1 || :
done

# Exercise source-contract-free auto-discovery failures before a child tool
# starts. The ordinary failure walk takes the source-contract path instead.
for index in $(seq 1 80); do
  P101_FAULT_CALL=$index P101_FAULT_ERRNO=5 \
    "$doctor" -x -o "$work/skip-fault-$index" "${common[@]}" -- /usr/bin/true \
    >/dev/null 2>&1 || :
done
