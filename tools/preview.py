#!/usr/bin/env python3
"""Render a page in SDL with simulated values, for states you cannot wait for.

`snapshot.py` renders what the YAML says, which for a page driven by Home
Assistant means every label sits at its placeholder -- a page full of "--" tells
you nothing about whether a rainy afternoon fits. This injects values into a
COPY of the page, renders it, and restores the original in a `finally`, so a
mock string can never reach a flash.

    tools/preview.py sdl-outside-mock.yaml tools/previews/outside-rain.json

A scenario is JSON:

    {
      "page": "layouts/480x320-home/pages/outside.yaml",
      "out":  "outside-rain.png",
      "labels":  {"out_temp": "62\\u00b0"},     # id -> text:
      "bars":    {"out_air_bar": 55},           # id -> value:
      "heights": {"out_rain_1": 6},             # id -> height:  (inserts if absent)
      "x":       {"out_sun_dot": 69},           # id -> x:
      "visible": ["out_rain_block"],            # drop  hidden: true
      "hidden":  ["out_dry_block"]              # ensure hidden: true
    }

Only ids that exist are accepted; a typo is an error rather than a silently
unchanged render, because a preview you cannot trust is worse than none.
"""
import json
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def block_of(text, wid):
    """Span of the widget carrying `id: <wid>`, up to the next sibling key."""
    m = re.search(r'^(\s*)id: ' + re.escape(wid) + r'\s*$', text, re.M)
    if not m:
        sys.exit(f'preview: no widget with id {wid}')
    indent = len(m.group(1))
    start = m.start()
    for line in re.finditer(r'^([ ]*)(?=\S)', text[m.end():], re.M):
        if len(line.group(1)) < indent or (
                len(line.group(1)) == indent and
                text[m.end() + line.end():m.end() + line.end() + 2] == '- '):
            return start, m.end() + line.start()
    return start, len(text)


def set_key(text, wid, key, value, insert_after=None):
    """Replace `key: ...` inside wid's block, or insert it if absent."""
    start, end = block_of(text, wid)
    body = text[start:end]
    pat = re.compile(r'^(\s*)' + key + r': .*$', re.M)
    if pat.search(body):
        body = pat.sub(lambda m: f'{m.group(1)}{key}: {value}', body, count=1)
    else:
        anchor = re.search(r'^(\s*)' + (insert_after or r'<<: \*\w+') + r'.*$', body, re.M)
        if not anchor:
            sys.exit(f'preview: cannot place {key} on {wid}')
        body = (body[:anchor.end()] +
                f'\n{anchor.group(1)}{key}: {value}' + body[anchor.end():])
    return text[:start] + body + text[end:]


def set_hidden(text, wid, hide):
    start, end = block_of(text, wid)
    body = text[start:end]
    has = re.search(r'^(\s*)hidden: true\s*$', body, re.M)
    if hide and not has:
        line = re.match(r'^(\s*)id: ', body)
        body = body.rstrip('\n') + f'\n{line.group(1)}hidden: true\n'
    elif has and not hide:
        body = re.sub(r'^\s*hidden: true\s*$\n', '', body, count=1, flags=re.M)
    return text[:start] + body + text[end:]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    config, scenario_path = sys.argv[1], sys.argv[2]
    scenario = json.loads(pathlib.Path(scenario_path).read_text())
    page = ROOT / scenario['page']
    backup = page.with_suffix('.yaml.preview-backup')
    shutil.copy(page, backup)
    try:
        t = page.read_text()
        for wid, val in scenario.get('labels', {}).items():
            t = set_key(t, wid, 'text', json.dumps(val))
        for wid, val in scenario.get('bars', {}).items():
            t = set_key(t, wid, 'value', val)
        for wid, val in scenario.get('heights', {}).items():
            t = set_key(t, wid, 'height', val)
        for wid, val in scenario.get('x', {}).items():
            t = set_key(t, wid, 'x', val)
        for wid in scenario.get('visible', []):
            t = set_hidden(t, wid, False)
        for wid in scenario.get('hidden', []):
            t = set_hidden(t, wid, True)
        page.write_text(t)
        r = subprocess.run([sys.executable, 'tools/snapshot.py', config],
                           cwd=ROOT, capture_output=True, text=True)
        sys.stdout.write(r.stdout)
        if r.returncode != 0:
            sys.stderr.write(r.stderr[-2000:])
            sys.exit(1)
        if 'out' in scenario:
            # the page's own snapshot, not the first one printed: a config
            # with several pages lists them in navigation order
            pid = re.search(r'^\s*pages:\s*\n\s*- id: (\w+)', t, re.M).group(1)
            src = re.search(r'^' + pid + r': (\S+\.png)', r.stdout, re.M)
            if not src:
                sys.exit(f'preview: no snapshot of page {pid}')
            dst = ROOT / 'snapshots' / scenario['out']
            shutil.copy(src.group(1), dst)
            print('saved', dst)
    finally:
        shutil.copy(backup, page)
        backup.unlink()


main()
