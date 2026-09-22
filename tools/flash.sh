#!/usr/bin/env bash
# Fork-only: build and OTA-flash a config, and prove the device runs the result.
#
#   tools/flash.sh home35.yaml
#   tools/flash.sh home35.yaml --expect 'home_page: lighting_second' --expect 'active: false'
#   tools/flash.sh home35.yaml --device 192.168.89.135
#   tools/flash.sh home35.yaml --dry-run      # everything but the upload
#
# Each step stops the run if it fails, in this order:
#   1. esphome config, and every --expect regex must match the resolved config
#      (the values you approved -- a half-applied edit can't slip through)
#   2. lint: tools/lint_configs.py on the config
#   3. compile, noting the build's "compiled on" time
#   4. find the device: --device, else <name>.local, else Home Assistant's
#      ESPHome entry for it (over ssh to the SSH add-on)
#   5. upload, with a fresh log -- never read a previous run's "OTA successful"
#   6. read the device's own "compiled on" and require it to match step 3
set -euo pipefail
cd "$(dirname "$0")/.."

config=""; device=""; dry=0; expects=()
while [ $# -gt 0 ]; do
  case "$1" in
    --device) device="$2"; shift 2 ;;
    --expect) expects+=("$2"); shift 2 ;;
    --dry-run) dry=1; shift ;;
    -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
    *) config="$1"; shift ;;
  esac
done
[ -n "$config" ] || { sed -n '2,17p' "$0"; exit 2; }

work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
step() { printf '\n== %s\n' "$*"; }
timed() { perl -e 'alarm shift; exec @ARGV' "$@"; }   # macOS has no timeout(1)

step "resolve $config"
esphome config "$config" > "$work/resolved.yaml" 2>&1 || { tail -20 "$work/resolved.yaml"; exit 1; }
for e in ${expects[@]+"${expects[@]}"}; do
  grep -Eq -- "$e" "$work/resolved.yaml" || { echo "expected /$e/ not in the resolved config"; exit 1; }
  echo "ok: /$e/"
done
name=$(awk '/^esphome:/{f=1;next} f&&/^  name: /{print $2; exit} f&&/^[a-z]/{exit}' "$work/resolved.yaml")
echo "device name: $name"

step "lint"
python3 tools/lint_configs.py "$config" | grep -v ' used, ' || true
python3 tools/lint_configs.py "$config" > /dev/null || { echo "lint failed"; exit 1; }

step "compile"
esphome compile "$config" > "$work/build.log" 2>&1 || { tail -30 "$work/build.log"; exit 1; }
grep -E 'RAM:|Flash:' "$work/build.log" || true
bin=".esphome/build/$name/build/firmware.ota.bin"
built=$(strings "$bin" | grep -oE '20[0-9]{2}-[0-9]{2}-[0-9]{2} [0-9:]{8} [-+][0-9]{4}' | head -1)
echo "build compiled on $built"

step "find the device"
if [ -z "$device" ]; then
  device=$(timed 5 dscacheutil -q host -a name "$name.local" 2>/dev/null | awk '/ip_address/{print $2; exit}' || true)
fi
if [ -z "$device" ]; then
  device=$(ssh -o BatchMode=yes -o ConnectTimeout=10 homeassistant "python3 -c \"
import json
for e in json.load(open('/config/.storage/core.config_entries'))['data']['entries']:
    d = e.get('data', {})
    if e['domain'] == 'esphome' and d.get('device_name') == '$name':
        print(d.get('host'))
\"" 2>/dev/null | head -1 || true)
fi
[ -n "$device" ] || { echo "can't find $name; pass --device"; exit 1; }
echo "device: $device"

if [ "$dry" = 1 ]; then
  step "dry run: not uploading"
else
  step "upload"
  esphome upload "$config" --device "$device" > "$work/upload.log" 2>&1 || { grep -v socket "$work/upload.log" | tail -15; exit 1; }
  grep -q 'OTA successful' "$work/upload.log" || { echo "no 'OTA successful' in this run's log"; exit 1; }
  echo "uploaded"
  sleep 20
fi

step "verify"
running=$( (timed 40 esphome logs "$config" --device "$device" 2>&1 || true) \
  | grep -oE 'compiled on 20[0-9]{2}-[0-9]{2}-[0-9]{2} [0-9:]{8} [-+][0-9]{4}' | head -1 | sed 's/compiled on //')
echo "device runs build compiled on ${running:-<no answer>}"
if [ "$running" = "$built" ]; then
  echo "OK: $name is running this build"
else
  [ "$dry" = 1 ] && { echo "(dry run: the device is on a different build)"; exit 0; }
  echo "MISMATCH: the device is not running the build just made"; exit 1
fi
