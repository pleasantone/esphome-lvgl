#!/usr/bin/env python3
"""Fork-only: prove no personal entity or fork-only file rides along upstream.

    tools/check_leaks.py                   # the current branch
    tools/check_leaks.py my-branch         # another one
    tools/check_leaks.py --base upstream/main my-branch

Run it before pushing any branch meant for RyanEwen/esphome-lvgl. It
collects the entity ids the personal (-home) layouts use, drops the ones
upstream's own layouts also use (it has light.bedroom_light_1 and the like),
and scans what the branch adds over the base -- added lines, with word
boundaries, so light.bedroom_light doesn't match light.bedroom_light_1 -- plus
the list of files it touches, for the files that never go upstream.

Exits 1 on any hit.
"""
import argparse
import re
import subprocess
import sys

DOMAINS = ("light|switch|sensor|binary_sensor|cover|fan|scene|media_player|"
           "alarm_control_panel|automation|script|climate|select|number|input_number")
ENTITY = re.compile(rf"\b(?:{DOMAINS})\.[a-z0-9_]{{3,}}\b")
FORK_ONLY = re.compile(r"^(CLAUDE\.md|home\d+\.yaml|sdl-home\.yaml|tools/split_layout\.py|"
                       r"tools/check_leaks\.py|tools/check_entities\.py|tools/flash\.sh|"
                       r"layouts/[^/]*-home(\.yaml|/.*)|layouts/fonts/glyphs-home.*)$")


def git(*a):
    return subprocess.run(["git", *a], capture_output=True, text=True, check=True).stdout


def entities_at(ref, pattern):
    files = [f for f in git("ls-tree", "-r", "--name-only", ref, "layouts/").split()
             if re.search(pattern, f) and f.endswith(".yaml")]
    found = set()
    for f in files:
        found |= set(ENTITY.findall(git("show", f"{ref}:{f}")))
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("branch", nargs="?", default="HEAD")
    ap.add_argument("--base", default="upstream/main")
    ap.add_argument("--home", default="main", help="where the -home layouts live (default: main)")
    args = ap.parse_args()

    mine = entities_at(args.home, r"-home[/.]")
    theirs = entities_at(args.base, r"^layouts/")
    only_mine = mine - theirs
    added = [l[1:] for l in git("diff", f"{args.base}...{args.branch}").splitlines()
             if l.startswith("+") and not l.startswith("+++")]
    hits = sorted({(e, l.strip()) for l in added for e in ENTITY.findall(l) if e in only_mine})
    files = [f for f in git("diff", "--name-only", f"{args.base}...{args.branch}").split()
             if FORK_ONLY.match(f)]

    print(f"{len(only_mine)} personal entities checked against "
          f"{len(added)} added lines in {args.base}...{args.branch}")
    for e, line in hits:
        print(f"  ENTITY {e}: {line}")
    for f in files:
        print(f"  FORK-ONLY FILE {f}")
    if hits or files:
        sys.exit(1)
    print("clean")


if __name__ == "__main__":
    main()
