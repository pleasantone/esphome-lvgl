#!/usr/bin/env python3
"""Fork-only: check every entity a config subscribes to exists in Home Assistant.

    tools/check_entities.py home35.yaml [more.yaml ...]
    tools/check_entities.py --ignore '_ams_(3|4|129)_' home35.yaml

The build can't catch a mistyped or deleted entity: Home Assistant sends
nothing for an entity that doesn't exist, and the tile sits on its unknown
glyph. This resolves each config, collects its entity_ids and asks Home
Assistant for all states in one call.

Access, first that works:
  HA_URL + HA_TOKEN in the environment (a long-lived access token), or
  ssh to the Advanced SSH add-on (--ssh, default "homeassistant"), whose
  shell has a SUPERVISOR_TOKEN for the core API.

The printers carry twelve AMS slots and most don't exist, so AMS entities
that are missing are listed separately and don't fail the run.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import urllib.request

ENTITY = re.compile(r"entity_id: ([a-z_]+\.[a-z0-9_]+)")
AMS_SLOT = re.compile(r"_ams_\d+_")


def ha_states(ssh_host):
    url, token = os.environ.get("HA_URL"), os.environ.get("HA_TOKEN")
    if url and token:
        req = urllib.request.Request(url.rstrip("/") + "/api/states",
                                     headers={"Authorization": f"Bearer {token}"})
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.load(r)
    cmd = ('curl -sf -H "Authorization: Bearer $SUPERVISOR_TOKEN" '
           "http://supervisor/core/api/states")
    r = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", ssh_host, cmd],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"couldn't read states over ssh {ssh_host} (set HA_URL and HA_TOKEN instead)\n{r.stderr}")
    return json.loads(r.stdout)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("configs", nargs="+")
    ap.add_argument("--ssh", default="homeassistant", help="host of the SSH add-on")
    ap.add_argument("--ignore", help="regex of entity ids expected to be missing")
    args = ap.parse_args()

    have = {s["entity_id"] for s in ha_states(args.ssh)}
    ignore = re.compile(args.ignore) if args.ignore else None
    failed = False
    for c in args.configs:
        r = subprocess.run(["esphome", "config", c], capture_output=True, text=True)
        if r.returncode:
            print(f"{c}: esphome config failed")
            failed = True
            continue
        wanted = sorted({e for e in ENTITY.findall(r.stdout) if "${" not in e})
        missing = [e for e in wanted if e not in have and not (ignore and ignore.search(e))]
        ams = [e for e in missing if AMS_SLOT.search(e)]
        real = [e for e in missing if not AMS_SLOT.search(e)]
        print(f"{c}: {len(wanted)} entities, {len(real)} missing"
              + (f", {len(ams)} absent AMS slot entities (expected)" if ams else ""))
        for e in real:
            print(f"  MISSING {e}")
        failed |= bool(real)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
