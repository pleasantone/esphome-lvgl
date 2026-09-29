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
#      ESPHome entry for it (over ssh to the SSH add-on). With
#      name_add_mac_suffix, every panel running the image: HA entries named
#      <name>-xxxxxx, plus a bare <name> still on the unsuffixed build.
#   5. upload, with a fresh log -- never read a previous run's "OTA successful"
#   6. read the device's own "compiled on" and require it to match step 3
# Steps 5 and 6 run per device; the run fails if any device fails.
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
suffix=0; grep -Eq '^  name_add_mac_suffix: true' "$work/resolved.yaml" && suffix=1
echo "device name: $name$([ $suffix = 1 ] && echo '-<mac>')"

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
devices=()
[ -n "$device" ] && devices=("$device")
if [ -z "$device" ]; then
  ip=$(timed 8 dscacheutil -q host -a name "$name.local" 2>/dev/null | awk '/ip_address/{print $2; exit}' || true)
  [ -n "$ip" ] && devices=("$ip")
fi
if [ -z "$device" ] && { [ ${#devices[@]} = 0 ] || [ $suffix = 1 ]; }; then
  while read -r h; do [ -n "$h" ] && [[ " ${devices[*]-} " != *" $h "* ]] && devices+=("$h"); done < <(ssh -o BatchMode=yes -o ConnectTimeout=10 homeassistant "python3 -c \"
import json, re
for e in json.load(open('/config/.storage/core.config_entries'))['data']['entries']:
    d = e.get('data', {})
    n = d.get('device_name') or ''
    if e['domain'] == 'esphome' and (n == '$name' or ($suffix and re.fullmatch('$name-[0-9a-f]{6}', n))):
        print(d.get('host'))
\"" 2>/dev/null | sort -u || true)
fi
[ ${#devices[@]} -gt 0 ] || { echo "can't find $name; pass --device"; exit 1; }
echo "devices: ${devices[*]}"

failed=0
for device in "${devices[@]}"; do
  if [ "$dry" = 1 ]; then
    step "$device: dry run, not uploading"
  else
    step "$device: upload"
    if ! esphome upload "$config" --device "$device" > "$work/upload.log" 2>&1; then
      grep -v socket "$work/upload.log" | tail -15; failed=1; continue
    fi
    grep -q 'OTA successful' "$work/upload.log" || { echo "no 'OTA successful' in this run's log"; failed=1; continue; }
    echo "uploaded"
    sleep 20
  fi

  step "$device: verify"
  running=$( (timed 40 esphome logs "$config" --device "$device" 2>&1 || true) \
    | grep -oE 'compiled on 20[0-9]{2}-[0-9]{2}-[0-9]{2} [0-9:]{8} [-+][0-9]{4}' | head -1 | sed 's/compiled on //')
  echo "device runs build compiled on ${running:-<no answer>}"
  if [ "$running" = "$built" ]; then
    echo "OK: $device is running this build"
  elif [ "$dry" = 1 ]; then
    echo "(dry run: the device is on a different build)"
  else
    echo "MISMATCH: $device is not running the build just made"; failed=1
  fi
done
exit $failed
