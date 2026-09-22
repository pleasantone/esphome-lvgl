"""Split a layouts/<WxH>.yaml into layout core + pages/<page>.yaml + vars/<name>.yaml + all.yaml.

The one-package-per-page conversion behind upstream #59. Fork-only; see CLAUDE.md.

Usage: split_layout.py <src layout> <repo root> <WxH> [page,page,...]
  cp layouts/480x320.yaml /tmp/480x320.orig.yaml
  python3 tools/split_layout.py /tmp/480x320.orig.yaml . 480x320
Reads the original (unsplit) layout and writes layouts/<WxH>.yaml, layouts/<WxH>/pages/,
layouts/<WxH>/vars/ and layouts/<WxH>/all.yaml. Prints the page order, which the top-level
configs must list. It does not edit the examples. The optional last argument names pages
that stay in the layout beside `splash` -- shared `skip: true` pages such as the light detail
pages, which tiles on many pages open and nobody should have to list.

Every `<<: *anchor` a page uses must be in VARS below; a sensors package is placed on the
page whose widget has its `uid`, falling back to the `# <page>` comment above it.
"""
import re
import sys
from pathlib import Path

src, root, res = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
KEEP_PAGES = {"splash"} | set(filter(None, (sys.argv[4] if len(sys.argv) > 4 else "").split(",")))
L = src.read_text().split("\n")

VARS = {
    "page_styles": ("page", "a page's padding"),
    "container_styles": ("container", "the container a page's widgets sit in"),
    "button_layout": ("button_layout", "the grid the buttons sit in"),
    "button_widget_vars": ("button", "a half-width button"),
    "wide_button_widget_vars": ("wide_button", "a full-width button"),
    "printer_layout": ("printer_layout", "the column the printer tiles sit in"),
    "printer_widget_vars": ("printer", "sizing inside a printer tile"),
    "printer_tile": ("printer_tile", "a printer tile"),
    "printer_tile_layout": ("printer_tile_layout", "the column inside a printer tile"),
    "printer_bar_vars": ("printer_bar", "a printer's progress bar"),
    "ams_row_vars": ("ams_row", "an AMS row"),
}
KEEP_IN_LAYOUT = {"nav_widget_vars"}

out_dir = root / "layouts" / res
(out_dir / "pages").mkdir(parents=True, exist_ok=True)
(out_dir / "vars").mkdir(parents=True, exist_ok=True)


def idx(pred, start=0):
    for i in range(start, len(L)):
        if pred(L[i]):
            return i
    raise ValueError("not found")


# --- .sizing anchors -> vars files ---------------------------------------
s0 = idx(lambda l: l == ".sizing:")
s1 = idx(lambda l: l and not l.startswith(" ") and l != ".sizing:", s0 + 1)
anchors = {}
cur = None
for l in L[s0 + 1 : s1]:
    m = re.match(r"^  - &([a-z_]+)$", l)
    if m:
        cur = m.group(1)
        anchors[cur] = []
    elif cur and l.startswith("    "):
        anchors[cur].append(l[4:])
for name, body in anchors.items():
    if name in KEEP_IN_LAYOUT:
        continue
    fname, desc = VARS[name]
    body = [re.sub(r"!include styles/", "!include ../../styles/", b) for b in body]
    (out_dir / "vars" / f"{fname}.yaml").write_text(f"# {res}: {desc}.\n" + "\n".join(body) + "\n")

# --- pages ---------------------------------------------------------------
p0 = idx(lambda l: l == "  pages:")
p1 = idx(lambda l: l and not l.startswith(" "), p0 + 1)
chunks = []  # (id, lines)
for i in range(p0 + 1, p1):
    m = re.match(r"^    - id: ([a-z0-9_]+)", L[i])
    if m:
        chunks.append([m.group(1), []])
    if chunks:
        chunks[-1][1].append(L[i])
keep_pages = [c for c in chunks if c[0] in KEEP_PAGES]
pages = [c for c in chunks if c[0] not in KEEP_PAGES]
for c in pages:
    while c[1] and not c[1][-1].strip():
        c[1].pop()


def rewrite(line):
    line = re.sub(r"<<: \[([^]]*)\]", lambda m: "<<: [" + ", ".join(
        f"!include ../vars/{VARS[a.strip().lstrip('*')][0]}.yaml" for a in m.group(1).split(",")) + "]", line)
    line = re.sub(r"<<: \*([a-z_]+)", lambda m: f"<<: !include ../vars/{VARS[m.group(1)][0]}.yaml", line)
    line = re.sub(r"(file: |!include )widgets/", r"\1../../widgets/", line)
    return line


# --- sensor packages -----------------------------------------------------
k0 = idx(lambda l: l == "packages:")
entries = []  # (section comment, lines)
section = None
for l in L[k0 + 1 :]:
    m = re.match(r"^  # (.+)$", l)
    if m:
        section = m.group(1).strip()
        continue
    if re.match(r"^  [a-z0-9_]+: ", l):
        entries.append([section, [l]])
    elif entries and l.strip() and l.startswith("  "):
        entries[-1][1].append(l)
layout_pkgs, page_pkgs = [], {c[0]: [] for c in pages}
for section, lines in entries:
    text = "\n".join(lines)
    if section in ("header", "boot screen"):
        layout_pkgs.append((section, lines))
        continue
    m = re.search(r"uid: ([a-z0-9_]+)", text)
    owner = None
    if m:
        for pid, cl in pages:
            if re.search(rf"uid: {m.group(1)}\b", "\n".join(cl)):
                owner = pid
    if owner is None and section in page_pkgs:
        owner = section
    if owner is None:
        raise SystemExit(f"cannot place package: {lines[0]}")
    page_pkgs[owner].append(lines)

for pid, cl in pages:
    title = None
    for l in cl:
        m = re.match(r"^\s+text: (.+)$", l)
        if m:
            title = m.group(1).strip()
            break
    head = [
        f"# {title or pid}: the page and the sensors its tiles need.",
        "# Include it from the top-level config after `layout:`; pages appear in the",
        "# order they are listed.",
        "",
        "lvgl:",
        "  pages:",
    ]
    body = [rewrite(l) for l in cl]
    pk = []
    if page_pkgs[pid]:
        pk = ["", "packages:"]
        for n, lines in enumerate(page_pkgs[pid]):
            if n:
                pk.append("")
            pk += [rewrite(l) for l in lines]
    (out_dir / "pages" / f"{pid}.yaml").write_text("\n".join(head + body + pk) + "\n")

# --- all.yaml ------------------------------------------------------------
allf = [
    f"# Every {res} page, in navigation order. A top-level config can include this",
    "# in place of listing its pages one by one:",
    "#",
    f"#   pages: !include layouts/{res}/all.yaml",
    "",
    "packages:",
] + [f"  {pid}: !include pages/{pid}.yaml" for pid, _ in pages]
(out_dir / "all.yaml").write_text("\n".join(allf) + "\n")

# --- the layout itself ---------------------------------------------------
new = []
i = 0
while i < len(L):
    l = L[i]
    if i == s0:
        new.append(l)
        for name, body in anchors.items():
            if name in KEEP_IN_LAYOUT:
                new.append(f"  - &{name}")
                new += ["    " + b if b else b for b in body]
        new.append("")
        i = s1
        continue
    if i == p0:
        new.append(l)
        for pid, cl in keep_pages:
            new += cl
        new.append("  # every other page is a package in layouts/%s/pages/, listed by the" % res)
        new.append("  # top-level config after layout: -- see the README")
        new.append("")
        i = p1
        continue
    if i == k0:
        new.append(l)
        for n, (section, lines) in enumerate(layout_pkgs):
            if n:
                new.append("")
            new.append(f"  # {section}")
            new += lines
        new.append("")
        break
    new.append(l)
    i += 1
# collapse runs of blank lines
txt = re.sub(r"\n{3,}", "\n\n", "\n".join(new)).rstrip("\n") + "\n"
(root / "layouts" / f"{res}.yaml").write_text(txt)
print(" ".join(pid for pid, _ in pages))
