# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A pure-YAML ESPHome configuration repo (no code, no build system of its own) that drives LVGL touchscreen
dashboards on cheap ESP32 display boards, backed by Home Assistant. Every file is ESPHome YAML consumed
through the `packages:` / `!include` mechanism.

## How main relates to upstream

main is **upstream/main + #61 + #68 + the fork's own commits**, rebuilt and force-pushed on 2026-09-22 as
if Ryan had merged those two (tags: `main-pre-rebuild-6168`, and `main-pre-rebase` from the 2026-09-21
rebuild). The fork's own copies of merged work (#62 BLE proxy, #63 steppers, #64 diagnostics, #65 sleep
clock, #67 the ILI9342 device file) were dropped in favour of upstream's.

**No shared file diverges any more**: every file upstream also has is byte-identical to upstream plus the
open PRs, so a merge of upstream/main is clean. Everything fork-only is a separate file (the list under
"Personal vs example layouts"). Keep it that way -- a fork-only note about a shared file belongs here, not
in that file.

Sync with a plain merge of upstream/main; rebuilding again would need another force-push, which is Paul's
call. When #61 and #68 land, `git log main..upstream/main` should show only Ryan's merge commits.

## Commands

```bash
esphome config <top-level>.yaml     # validate + dump the fully-substituted config (fastest feedback loop)
esphome compile <top-level>.yaml    # compile firmware
esphome run <top-level>.yaml        # compile + upload (USB or OTA)
esphome run sdl-example.yaml        # run the UI on the host in an SDL window — no hardware needed
esphome logs <top-level>.yaml

tools/lint_configs.py [cfg ...]       # undefined ${var}s, missing glyphs, page_access includes (#70)
tools/compare_configs.py --base REF   # every config resolved at REF vs the tree; pages + line diff (#70)
tools/snapshot.py sdl-example.yaml    # a PNG per page, no hardware, no HA (#70)
tools/preview.py sdl-home.yaml tools/previews/outside-rain.json   # fork-only: a page with simulated values
tools/check_glyphs.py cfg ...         # MDI subset: used-but-missing (blank) and unused (#70)
tools/check_leaks.py <branch>         # fork-only: personal entities / files headed upstream
tools/check_entities.py home35.yaml   # fork-only: every subscribed entity exists in HA (via ssh)
tools/flash.sh home35.yaml --expect 'regex' ...   # fork-only: gated build + OTA + verify
```

**Flash with `tools/flash.sh`, not a hand-chained `esphome upload`.** It encodes the OTA lessons of
2026-09-21/22: every `--expect` regex must match the resolved config (the approved values), the lint must
pass, the upload log is fresh (a stale "OTA successful" was once reported as a flash), the device is found
by `.local` or by Home Assistant's ESPHome entry, and it ends by reading the device's own "compiled on"
and requiring it to equal the build's. `--dry-run` does all but the upload.

Only the top-level files in the repo root (`*-example.yaml`) are directly buildable; everything under
`devices/` and `layouts/` is a package fragment that fails on its own.

`esphome config` is the primary debugging tool here: because the layouts are built from anchors, merge keys
and parameterized includes, the resolved output is the only reliable way to see what a widget actually
becomes.

Non-SDL builds need a `secrets.yaml` next to the config with `wifi_ssid` and `wifi_password` (not in the
repo). `sdl-example.yaml` deliberately skips `common.yaml`, so it needs no secrets and no WiFi.

## Composition model

A top-level config is only a few lines — it names the device, picks a home page, merges three packages,
and opts into any features:

```yaml
esphome: { name: ..., friendly_name: ... }   # device_name/friendly_name live HERE, not in substitutions
substitutions: { home_page: lighting_1 }
packages:
  common: !include common.yaml               # WiFi, API, OTA, web_server, uptime + reset reason
  device: !include devices/<BOARD>.yaml      # pins, display, touchscreen, backlight, psram, framework
  layout: !include layouts/<WxH>.yaml        # fonts, theme, LVGL pages, and all HA sensors
                                             # (or <WxH>-home.yaml -- see "Personal vs example layouts")
  idle: !include features/idle/idle.yaml     # optional behaviour, AFTER layout: -- see "Features"
```

**A layout's name is the canvas it was drawn for, not what every board gives it.** The Guition
`JC3248W535` is natively **320x480 portrait** (see `width=320, height=480` in ESPHome's
`mipi_spi/models/jc.py`) and this repo sets no rotation for it, so the `480x320` layouts are actually
driven at 320px wide on that board — about 298px usable inside the page and tile padding. Budget widths
against the real canvas, not the filename. Boards that need landscape set `lvgl: rotation:` in their
device file.

The axes are intentionally orthogonal: `devices/` files contain *only* hardware — pins, display,
touchscreen, backlight, and stable ids for them — and nothing UI-related or behavioural (Ryan's rule, from
upstream PR #50: device files stay project-agnostic); `layouts/` files contain nothing board-specific; and
behaviour a panel may or may not want lives in `features/`. Layouts are keyed by resolution (`320x240`, `480x320`, `800x480`), so
several boards share one layout. Adding a board means adding one file to `devices/` and nothing else.

`home_page` is a required substitution consumed by the `go_home` script in every layout; it is also what the
footer home button triggers.

### Personal vs example layouts

`layouts/<WxH>.yaml` is the **generic example**, kept as close to upstream as the widget set allows: demo
entities (`light.kitchen_light`, printers Fred/Wilma/Barney on `p1s_N`) that no real house has. It is what
every `*-example.yaml` builds, and what can be offered upstream.

`layouts/<WxH>-home.yaml` is the **personal** one -- this author's real lights, alarm, covers and Bambu
serials -- with its pages in `layouts/<WxH>-home/pages/`, split the same way as the generic layouts.
Only `home35.yaml` and `sdl-home.yaml` use it, and list its pages.

The split exists because the personal layout had once overwritten the example, so anyone cloning the repo
and flashing `guition-35-example` got a panel wired to entities they do not own. **Put personal entities in
a `-home` layout; leave `layouts/<WxH>.yaml` generic.**

The generic layouts keep upstream's page names (`lighting_1`/`_2`/`_3`), so the examples keep
`home_page: lighting_1`. The personal `480x320-home.yaml` renamed them to `lighting_main` / `lighting_second`
/ `lighting_ground` / `lighting_outside`, which is why `home35.yaml` and `sdl-home.yaml` differ there.

The generic layouts do not carry the old `widgets/printers/widget.yaml`; their printers pages were
recomposed onto `tile_combined.yaml` + `printer_combined.sensors.yaml`, which reproduces upstream's
single-AMS combined line using the current widget set.

There are two `-home` layouts, `480x320-home` (home35, sdl-home) and `320x240-home` (home28); the personal
`800x480` was deleted as an unbuilt copy, since Paul has no such board. Its history is in git.

**Before offering anything upstream, prove no personal entity rides along:** `tools/check_leaks.py
<branch>`. It collects the entities the `-home` layouts use, subtracts the ones upstream's own layouts
have, and scans the lines the branch adds over `upstream/main` with word boundaries (a substring match on
`light.bedroom_light` would hit upstream's own `light.bedroom_light_1`), plus the file list for anything
fork-only. Exit 1 on a hit; say what it printed rather than asserting the branch is clean.

Also never send `CLAUDE.md`, `home35.yaml`, `home28.yaml`, `sdl-home.yaml`, `tools/split_layout.py`,
`tools/check_leaks.py`, `tools/check_entities.py`, `tools/flash.sh`, `tools/previews/`,
`layouts/fonts/glyphs-home*`, the `-home` layouts or their `layouts/*-home/` directories upstream. They
are fork-only. (`tools/preview.py` itself is generic and could be offered upstream one day; only the
scenarios under `tools/previews/` name personal pages.)

## Layout file anatomy (`layouts/<WxH>.yaml`)

Each layout file is the single place where resolution-dependent numbers live, and follows a fixed shape:

1. `font: !include fonts/fonts.yaml` with `vars:` giving the pixel sizes for the `roboto_*` and `mdi_*`
   font ladders (sm/md/lg/xl/xxl). The MDI font is downloaded from Pictogrammers and subset by
   `layouts/fonts/glyphs.yaml` — **any new icon must be added to `glyphs.yaml` or it renders blank.** An icon
   only the personal pages use goes in `glyphs-home.yaml` instead (see the audit under "Checks").
2. A `.sizing:` block of YAML anchors (`*page_styles`, `*container_styles`, `*button_layout`,
   `*button_widget_vars`, `*printer_widget_vars`, …). Top-level keys starting with `.` are ignored by
   ESPHome, which is what makes this legal. Anchors merge a shared style file from `layouts/styles/` and
   then override the resolution-specific fields.
3. The `script: go_home` definition.
4. `lvgl:` — theme, a `top_layer` of header/footer/boot-screen widgets, and the `pages:` list.
5. A trailing `packages:` block that includes one `sensors.yaml` per stateful widget on the pages above.

Anchors are used instead of LVGL `style_definitions`/`styles` because anchors work on any node and, unlike
`styles`, actually override `theme` values. Files under `layouts/styles/` keep the resolution-independent
half of each style, with the resolution-specific keys left as `# RESOLUTION-SPECIFIC` comments documenting
what the layout must supply.

## Widget convention (`layouts/widgets/`)

A widget is a fragment included with `vars:`, merged in with `<<:`. Its first lines are a `# required vars:`
comment block listing every `${var}` it needs, and the ESPHome key it plugs into (`# button:`, `# obj:`) is
left commented out at the top. Follow both conventions when adding widgets.

Stateful widgets come in pairs that must be included together with the **same `uid` and `entity_id`**:

- `widget.yaml` / `stateful.yaml` — the LVGL tree, included inside a page under `lvgl.pages`.
- `sensors.yaml` / `stateful_sensors.yaml` — the `binary_sensor`/`sensor`/`text_sensor` platform
  `homeassistant` entries that drive it, included in the layout's bottom `packages:` block.

All widget child IDs are derived from `uid` (`${uid}_widget`, `${uid}_widget_on`, `${uid}_widget_off`,
`${uid}_widget_unknown`, …); the sensor file shows/hides those IDs by name. A mismatched `uid` produces a
missing-ID error at validation time, not a silent failure.

That check only ran in one direction — the sensors file naming a widget that does not exist. The reverse,
**a widget whose sensors package was never included**, used to render fine and sit on the unknown glyph
forever with nothing said at build time. Every stateful widget therefore names its own sensor once:

```yaml
- lambda: (void) id(${uid}_widget_sensor);
```

placed first in a handler the widget already has, or in an `on_click` on the status tiles, which are `obj:`
and never fire one. ESPHome resolves that id at config time, so the pair is now enforced both ways, and
pairing the wrong sensors file with a family fails too (plain light sensors under a `dimmable` tile trips
on the missing `${uid}_brightness`). It costs 736 bytes of RAM for the trigger objects.

A widget that already names its own sensor elsewhere needs no such line. `toggle_buttons/confirm_off.yaml`
gates on `binary_sensor.is_on: ${uid}_widget_sensor`, and that takes a `cv.use_id`, so a forgotten package
already fails there — verified by deleting the `x1c_plug` sensors package and watching the config be
rejected with `Couldn't find ID 'x1c_plug_widget_sensor'`. Check for an existing reference before adding
the lambda.

The pair exists because the two halves live in different top-level sections: a fragment merged into
`lvgl:` cannot also contribute `binary_sensor:` entries — only a **package** can contribute both. That is
what makes the duplication structural rather than lazy, and it is worth knowing before anyone tries to
remove it again. Converting tiles to one-package-each does work (`!extend` reaches a named grid, and
substitutions cross package boundaries where anchors cannot), but it appends rather than inserts, so tile
order silently becomes package order, and it does not close the gaps below. It was tried and reverted.

**Package-per-page is how pages ship now** (upstream #59): `layouts/<WxH>/pages/<page>.yaml` holds a page
*and* the sensor packages its tiles need, `layouts/<WxH>/vars/` holds what the `.sizing` anchors were (anchors
don't cross files), `all.yaml` lists them all, and the top-level config's list *is* the navigation order.
`tools/split_layout.py` does the conversion from an unsplit layout (usage in its docstring); every file in
#59 was its output. Re-split from a merge with upstream, never from a PR's own old layouts.

The ordering rules that shape all of this: **a package's pages merge in package order, and a package
included inside a file is processed before that file's own content**, so a page a layout includes lands
ahead of the layout's own pages. A feature listed after `layout:` therefore appends its page at the end,
which is how `features/sleep_clock/` brings one. There is no escape hatch for ordering: `- !include
pages/printers.yaml` under `pages:` returns only a page mapping, and a mapping merged into `lvgl:` cannot
contribute `binary_sensor:` entries. Boot no longer depends on any of it (see "Boot-time page selection").

Two moves still open, both worth doing when the detail page goes upstream: **detail pages as a package**
(`detail_light` / `detail_rgb` plus their globals, included once per layout, not per tile) and, folded into
that pass, **the confirm dialog as a package** (`confirm_box` + `confirm_entity` into the `confirm_off`
family; a package can always contribute `msgboxes:`). Still rejected: **package-per-tile**, which loses tile
order within a grid because `!extend` appends. Not worth doing: removing `splash`, without which
`detail_light`'s `on_load` runs at boot against empty globals.

The check for any such conversion: each config's resolved output against the old one -- identical as a
multiset and the same page ids in the same order. `tools/compare_configs.py --base <ref>` does it.

The five ways a pair breaks, and what catches each:

| failure | caught by |
|---|---|
| sensors package forgotten | the guard, since 2026-09-19 |
| wrong sensors file for the family | the guard, via the id it fails to find |
| `uid` typo in the sensors file | always has been — the sensors name widget ids |
| `entity_id` var omitted | **nothing.** An undefined `${var}` is only a warning, so the tile subscribes to the literal string |
| `entity_id` typo'd to an absent entity | **nothing.** Home Assistant sends nothing for an entity that does not exist, and says nothing about it |

The last two share one symptom — a tile stuck on the unknown glyph — so treat that glyph as "check the
entity_id", not "check the wiring".

**An on tile is distinguished by its glyph, not by a colour or an edge**, because every tile passes a real
`icon_on` / `icon_off` pair. A gold left edge was tried and removed (tag `gold-edge`): it cost 4px of content
width in every state, since `lv_obj_get_style_space_left()` adds `border_width` whenever `border_side`
includes `LEFT`, painted or not -- and gold on white is 1.40:1, far under the 3:1 floor for a non-text
indicator. If a same-glyph tile ever appears, give it an `icon_off` before reaching for colour.

The button tree specializes by shape and then by function: `buttons/{icon,text,icon_text}_buttons/` provide
generic `stateless.yaml` / `stateful.yaml` bases, and subdirectories (`light_buttons/`,
`light_group_buttons/`, `volume_buttons/`, `cover_buttons/`) wrap a base via `<<: !include ../stateful.yaml`
and bind the Home Assistant action. Prefer extending this chain over writing a new standalone widget.

`light_buttons/themed.yaml` is the light button with `icon_on` / `icon_off` as vars;
`light_buttons/widget.yaml` is that same file pinned to the lightbulb pair, so the original
five-var contract still holds. Pass the icons only where a tile wants to match what Home Assistant
shows for the entity.

`cover_buttons/` is driven by a `text_sensor`, not a `binary_sensor`: ESPHome's `homeassistant`
binary_sensor maps only the literal `"on"` to true, so a cover's `open` would read as false. It
actuates on `on_long_press` alone — a stray touch on a wall panel must not move a garage door.

`toggle_buttons/` is the domain-agnostic pair: `widget.yaml` takes a `domain` var and fires
`${domain}.toggle`, which covers a `switch`, a `fan` with no speed control, and an on/off-only
`light` without a file for each. Its `sensors.yaml` is a one-line wrapper around the shared
`icon_text_buttons/stateful_sensors.yaml`, since an on/off entity needs nothing extra.

`toggle_buttons/confirm_off.yaml` is that tile with a guard: turning the entity **off** opens a
confirm dialog, turning it back on is immediate. Only the destructive direction asks, which is why
it reads the tile's own `${uid}_widget_sensor` in the click handler rather than sending a blind
`toggle`. It is what the printer plugs use — a stray touch on a wall panel must not cut power to a
running print.

The dialog lives in the layout under `lvgl: msgboxes:` rather than in a widget file, because
`msgboxes:` is a top-level LVGL key. One `confirm_box` serves every guarded tile, on the
`detail_light` pattern: the tile stashes the entity in the `confirm_entity` global and writes the
question into the `confirm_body` label, then shows the box. The OK button calls
`homeassistant.turn_off`, which is domain-agnostic, so it acts on whatever `confirm_entity` holds.
`close_button: false` makes a tap anywhere outside the box cancel, so there are two ways out and one
way through. The footer buttons carry an explicit `width: 46%` / `height: 64`: a msgbox footer button
defaults to roughly text-plus-padding, which is about half a fingertip on a 320px wall panel. They
set `radius` to their own height the way the AMS tray pills do — LVGL clamps radius to half the
shorter side, so any value at or above half gives a full pill. Cancel sets no colour or font at all
and inherits the theme (`0x5A6173`, `roboto_md`); only the destructive button is recoloured, to
`0xFF3B30`, the red `status/alarm/` already uses for a triggered alarm. Reach for a colour the
palette already has rather than introducing one. `lvgl.widget.show` / `hide` on a msgbox id resolves to its full-screen outer overlay
(`widget.outer or widget` in `lvgl/automation.py`), which is what dims the page behind it.

`widgets/status/` holds tiles that report without controlling. They plug in under `obj:` rather than
`button:`, which the theme renders darker (`0x42495A` vs `0x5A6173`) so they read as not pressable.
`status/binary/` reuses the buttons' `stateful_sensors.yaml` and takes `icon_on` / `icon_off` /
`color_on`; `status/value/` shows a rounded numeric sensor plus a `unit` suffix; `status/alarm/` maps
an `alarm_control_panel` state string to a label and recolours its shield.

`status/threshold/` is `status/binary/`'s tree driven by a numeric sensor instead of an on/off
entity: above `threshold` is on, at or below it is off, and NAN (an unavailable entity) keeps the
unknown glyph rather than reading as off. It exists because **a smart plug's `switch` state answers
the wrong question** — `switch.clothes_dryer` is on whenever the outlet is live, so the Dryer tile
was lit while the dryer sat idle drawing 1.1W. The tile now reads `sensor.clothes_dryer_power`
against 5W.

That 5W is `end_appliance_power` in the Blackshome appliance-notifications blueprint behind
`automation.clothes_dryer`, which is what fires the "finished" notification, so tile and message
agree. Both power setpoints there are blueprint **defaults** and appear nowhere in the automation's
own config — the only value it overrides is `running_dead_zone: 10`, which is ten *minutes* of
start-up grace, not a wattage. Read the blueprint's inputs, not just the automation, before matching
a threshold to it.

### Stepper tiles (`widgets/stepper/`) — upstream #63, merged

`[-] value [+]` on an `obj` tile; `stepper/climate/` sets a thermostat's single target, `stepper/number/`
a `number` / `input_number`. Taps move a `${uid}_pending` global and restart a `${uid}_nudge` script that
sends one call 1s after the last tap, then takes HA's value back 3s later (in case it clamped or refused).
While the script runs, inbound HA updates are ignored so the display doesn't jump back. Range comes from
the entity's attributes; climate's `step` is a var, since thermostats rarely report one. `off` and
`heat_cool` have no single target: the tile shows `--` and the buttons do nothing.

The tile is a flex row: the name column `flex_grow`s into what the buttons leave, and the name label is
`long_mode: DOT` with `max_height: 50%` — DOT only truncates when the height is bounded; with
`SIZE_CONTENT` it wraps under the buttons instead. Sizes per canvas are in `vars/stepper.yaml` (buttons
40/52/80, value 52/60/88 for 240/320/480px), checked with SDL snapshots against "21.5°", "100%" and a
"Thermostat" name. The personal layout's Climate page drives `climate.upstairs`.

### The shared light detail page

Long-pressing a dimmable light opens `detail_light` — one `skip: true` page shared by all 13 of them,
not a page each. The long-press handler in `light_buttons/dimmable.yaml` stashes the entity, the title
and `get_current_page()` in globals, seeds the slider from that light's own `${uid}_brightness` sensor,
then shows the page. Back calls `main_lvgl->show_page(id(detail_origin), ...)`, because
`lvgl.page.show` takes a `cv.use_id` and cannot be templated.

Three things here are load-bearing and easy to get wrong:

- **`on_change` / `on_release`, never `on_value`.** `on_value` is wired to `VALUE_CHANGED` *and*
  ESPHome's private `UPDATE_EVENT`, which `lvgl.*.update` fires — so a slider on `on_value` would
  echo every state HA pushed back to it. `on_change` and `on_release` see only real user input
  (`lvgl/schemas.py`, `TRIGGER_EVENT_MAP`).
- **`attribute: brightness` publishes NAN while a light is off**, because HA sends the string
  `"None"` and the sensor logs a warning and publishes NaN. Every inbound lambda guards with
  `std::isnan(x)`; without it the NaN reaches `lv_slider_set_value(int32_t)` as undefined behaviour.
  The warnings at boot are expected and harmless.
- **`dimmable.yaml` drops `brightness_pct` from its short click.** `themed.yaml` still sends
  `'100'` on toggle, which would undo the slider on the next tap. Lights restore their own last
  brightness without it.

RGB lights get `detail_rgb` instead, via `light_buttons/rgb.yaml`: the same brightness slider plus a
hue arc, a saturation arc and a roller of curated WLED effects. **An `arc` defaults to
`adjustable: false`**, which renders it with no knob and no touch response — set it explicitly or the
page looks right and does nothing. Colour and effect are write-only here: HA reports `hs_color` as a
list and `effect` as a name, and a `homeassistant` sensor can feed back neither.

`homeassistant.action` `data:` values are templatable (`cv.templatable` in `api/__init__.py`), which
is what lets one page drive any light via `entity_id: !lambda return id(detail_entity);`. Values are
sent as strings, so scalars like `brightness_pct` work but a list-valued key such as `hs_color` does
not — that needs `data_template:` with `variables:`, which HA renders into a real list, as the hue/sat
arcs do with `hs_color: "{{ [hue | int, sat | int] }}"`.

The `roboto_*` fonts set no `glyphs:`, so they carry ESPHome's `GF_Latin_Kernel` default — `°` is in
it, unlike the MDI subset. Only `mdi_*` is restricted to `glyphs.yaml`.

## Printer tiles (`layouts/widgets/printers/`)

These do not follow the simple widget/sensors pairing above; they are split three ways so that the number
of AMS units is a composition choice rather than a fork of the whole tile:

- `sensors_core.yaml` — status / remaining time / end time / progress. One per printer.
- `ams_row.sensors.yaml` (4 trays) and `ams_row_single.sensors.yaml` (1 tray) — one per **AMS unit**,
  selected by `ams_id`, which is the unit number as it appears in the entity_id (`1`, `2`, and `128` for an
  AMS HT). Include one package per unit.
- `ams_row_humidity.sensors.yaml` — every AMS reports humidity, so pair it with every unit.
- `ams_row_drying.sensors.yaml` — **only** for units that have drying hardware. It has no widget of its own;
  it recolours that unit's humidity value amber while drying. Capability belongs to the AMS, not the
  printer, so if a unit is moved to another machine this package moves with it under the new
  `entity_id_prefix`. A unit that cannot dry omits the package and subscribes to nothing that does not
  exist — the H2C's three units all dry, the X1C's AMS has no `_drying` entity at all.
- `ams_row.yaml` (4 pills) / `ams_row_single.yaml` (1 pill) — the **full-width row** for one unit, plus
  `tile_status.yaml` (status / remaining / end) and `tile_progress.yaml` (the bar).

There used to be deliberately **no per-topology tile file**, because ESPHome YAML has no loops and a tile
file would have had to hardcode a unit count and fork for every combination (1 AMS, 2 AMS, 2 AMS + HT,
2 HT…). **Carrying all twelve slots on every printer retired that objection** — the topology is now
discovered at runtime rather than spelled out, so one body fits every machine. `800x480` still enumerates
(see below) and so still composes its tiles inline.

`layouts/480x320-home/pages/printers.yaml` therefore composes a tile from `vars/printer_tile.yaml` +
`vars/printer_tile_layout.yaml` and hands
`widgets:` the list in `printers/tile.yaml`, which holds the name label, all twelve rows, status and
progress. Its sensor side is one `printers/printer.sensors.yaml` per printer, which bundles
`sensors_core.yaml` plus twelve `ams_unit.sensors.yaml` (itself trays + humidity + drying). Adding a
printer is three lines on the page and four in `packages:`.

**The tile body is a per-printer choice.** `tile.yaml` is the stacked format described above.
`tile_combined.yaml` is upstream's original shape — the printer name and one unit's four pills share a
single line — and costs one line less. It is paired with `printer_combined.sensors.yaml`, which subscribes
to the core sensors and AMS 1's trays only. On `480x320` the X1C runs combined and the H2C stacked.

Two things follow from sharing the line, and both are why this is a choice rather than a default:

- **It cannot reveal itself.** The stacked rows are shown by their humidity sensor; hiding a combined line
  would take the printer name with it, so the line is statically visible. That suits a machine whose one
  AMS is always there and nothing else.
- **It is single-AMS, permanently.** A unit added to a combined printer will not appear — the adaptive
  slots are exactly what was traded for the line. Move that printer back to `tile.yaml` +
  `printer.sensors.yaml` if it grows a second unit.

The two halves are enforced against each other the usual way: `tile_combined.yaml` has no
`${uid}_ams_1_humidity_lbl` or `${uid}_ams_1_heater_icon`, so pairing it with the stacked
`printer.sensors.yaml` fails at config time on the ids it cannot find.

The four tray pills live in `ams_tray_strip.yaml`, shared by `ams_row.yaml` and `tile_combined.yaml`, so
pill geometry is written once.

Two ESPHome mechanics make that nesting work, both verified on the pinned 2026.9.0:

- **An include inherits the vars of the file that included it.** A nested `!include` needs no
  `vars: { uid: "${uid}" }` pass-through — `uid`, `entity_id_prefix` and the `ams_*` sizing numbers reach
  the leaf files on their own. Anchors still do not cross a file boundary, which is why the layout passes
  `*ams_row_vars` and `*printer_bar_vars` as vars once per tile with `<<: [*a, *b]`.
- **The include filename is substituted too**, so `!include ams_row${variant}.sensors.yaml` picks the
  4-tray or 1-tray file from a var instead of forking the call site.

`layouts/480x320-home/pages/printers.yaml` does not enumerate the units a printer actually has. **Each printer carries all
twelve slots** — AMS `1`–`4` and AMS HT `128`/`129` — and every row is included (in `printers/tile.yaml`)
with `hidden: true` as a sibling key of the merge. `ams_row_humidity.sensors.yaml` calls `lvgl.widget.show` on
`${uid}_ams_${ams_id}_row` when a reading arrives, so **a unit reveals its own row**: every AMS reports
humidity, a slot whose entities do not exist never sends anything and stays hidden, and an AMS moved
between printers appears on the new one without a reflash. Rows never hide again — a unit that goes away
leaves its row until the panel restarts. This costs about 9.6KB of RAM and 45KB of flash over
enumerating only the real units, which is the whole reason it is affordable.

Hiding a *row* is fine; the alignment caveat below is about hiding a column *within* a row.

Drying is subscribed for every slot for the same reason: a unit with no heater has no `_drying` entity, so
its heater icon stays blank on its own rather than because a package was left out. The capability is
discovered, not declared.

`layouts/800x480.yaml` is upstream's six demo printers, recomposed onto the current widgets. It enumerates
rather than using the self-revealing slots, which is the right call on a 480x800 canvas showing two printers
side by side. Upstream branches
keep the enumerated form too: the row `id` and the `lvgl.widget.show` are harmless where rows are always
visible, and only the layout's `hidden: true` opts into the adaptive behaviour.

Every tray id is `${uid}_ams_${ams_id}_tray_N_*`, so a unit is identified purely by `uid` + `ams_id`.

Nothing in an AMS row is ever hidden and every column has a fixed width: hiding a child removes it from
the flex row and shifts the pill columns out of alignment.

Right-justification is done by giving each column its own container with `flex_align_main: END`.
**Do not use `text_align` for this** — it is a valid property but was observed not to take effect here,
which is what left a wide gap between the humidity value and its glyph. The unit title is the exception:
its column uses `flex_align_main: START` so the titles line up on the left.

The humidity column holds three labels — heater icon, value, `water-percent` glyph — packed right. The
heater label starts empty and is written **only** by `ams_row_drying.sensors.yaml`, which is what gives it
three states without a "has a heater" var: blank when that package is absent, grey `0x9AA0AE` when the unit
has drying hardware but is idle, amber `0xFFAF00` while it dries. An empty label is zero-width, so a row
with no heater still lines its value up with the rows that have one.

Unit titles are the bare `1` / `2` / `HT`, not `AMS 1`. Spelling them out costs 38px on `480x320` — enough
to force `ams_tray_padding` down to 3 and the pill strip to 169 to buy it back, leaving 2px of slack in the
row. It is not worth it: the titles sit beside pills that already read as one unit's row. `ams_tray_padding`
is doing double duty as the gap between pills *and* between the three column groups, so trading it away
costs four times over. The pills cannot fund anything either — `100%` is 39px inside a 40px pill.

Row alignment across AMS units relies on `ams_strip_width` — a fixed-width pill strip (`4 * ams_tray_width
+ 3 * ams_tray_padding`) with its pills left-aligned inside. That is what makes a 1-pill AMS HT row sit
under pill 1 of the rows above rather than drifting to the right edge.

Humidity is clamped to 99 so its column only ever has to fit two digits.

`ams_label_width` / `ams_hum_width` must fit the rendered text or LVGL wraps it to two lines and doubles
the row height. Measure rather than guess, e.g. with PIL against the cached font in `.esphome/font/` —
`"HT"` is 21px and `"100%"` 39px at `roboto_sm` 16px.

The MDI font is **subset to exactly the characters in `layouts/fonts/glyphs.yaml`**, which does not include
a space. Blanking an `mdi_*` label with `text: " "` therefore renders a missing-glyph box, not blank space;
use `text: ""`. Look codepoints up rather than recalling them — the `post` table of the cached MDI TTF maps
glyph names to codepoints (`water-percent` is `U+F058E`; `U+F058C` is plain `water`).

### Bambu entity naming

The integration namespaces trays by unit: `sensor.<prefix>_ams_<n>_tray_<m>`, **not** `_ams_tray_<m>`.
`remaining_time` is a float in **hours** (`"1.5"`), and `end_time` is an ISO timestamp in **UTC** — so the
tile derives the displayed end time from `ha_time.now() + remaining_time` instead of reading `end_time`'s
hour field, which would be wrong by the local UTC offset. A tray with no RFID reports `remain: -1`.

## The Outside page (fork-only) — 2026-09-23

`layouts/480x320-home/pages/outside.yaml` is a glance page: time, the walk verdict, daylight, temperature,
the three air indexes, and a rain strip. It is on `home35` between Climate and Lighting Main. Its data
comes from two Home Assistant packages this repo does not contain — `/config/packages/panel_outside.yaml`
(rain, sun, pollen, the walk verdict, today's high/low) and `/config/packages/air_quality.yaml` (Google
Air Quality PM10, and a local dust-conditions model). Renaming a sensor there silently blanks a label
here; `tools/check_entities.py` is what catches it.

**A page file is a package, so it can carry its own `sensor:` and `text_sensor:` blocks.** The widget /
sensors pairing exists only because a fragment merged into `lvgl:` cannot contribute sensors — a page
file is not such a fragment. A bespoke page with ~20 subscriptions is clearer with its wiring inline than
split across a widget pair that nothing else will ever reuse.

What is load-bearing, and all of it was found by rendering rather than by reasoning:

- **The page hides the header** (`on_load`/`on_unload` on `titlebar`) and overrides `pad_top: 0`, since the
  shared page vars reserve 50px for a header this page does not draw. A sibling key beats a merge key.
  Safe alongside `features/sleep_clock/`, which hides the same widget: LVGL fires the outgoing screen's
  `SCREEN_UNLOAD_START` before the incoming screen's `SCREEN_LOAD_START` (`lv_display.c`), and ESPHome maps
  `on_unload`/`on_load` to exactly those, so the clock's hide always lands after this page's show.
- **The theme styles `obj:`, which does not reach a screen.** A page therefore keeps LVGL's near-white
  default. Every other page hides that behind the titlebar; this one showed it as a white band the moment
  `pad_top` was non-zero. The page sets its own `bg_color`.
- **A scrollbar means content overflows its box**, not that someone asked for scrolling. A 48px font has a
  ~56px line height, and a marker nudged 3px past the bottom edge to centre it on a bar is still 3px of
  overflow. Every block sets `scrollable: false`, but that only hides the symptom — fix the height too.
- **Exactly one of the rain strip and the dry line is visible, always.** A hidden strip must mean "dry",
  never "the forecast failed and nobody noticed". The dry line starts visible saying nothing is known, and
  the parser checks the string arrived at all before believing twelve zeroes — an unavailable entity
  publishes an empty string, which parses to a perfectly good dry forecast.
- **The walk verdict is a 300px contract.** Every branch of the verdict is measured against `roboto_lg` and
  must fit; the longest is `Rain in 115 min — go now` at 273px. Where a string might still overflow, put
  the actionable half first, because `long_mode: DOT` keeps the front. Do not reach for a scrolling label:
  moving text never settles, and this page is meant to be absorbed in a glance.
- **`long_mode: DOT` only truncates when the height is bounded.** With `SIZE_CONTENT` it wraps instead, and
  the verdict's second line lands on top of the narrative. Both labels carry an explicit `height:`.
- **Three lines beside the temperature sit in their own flex column on `SPACE_EVENLY`**, not pinned to
  TOP/MID/BOTTOM. Pinning left them touching, and their spacing then depended on the block height rather
  than on anything settable.
- `roboto_temp` (56) and `roboto_clock` (40) are declared in the page file, because the layout's ladder
  jumps 32 → 48 and `widgets/detail/light_page.yaml` also draws at `roboto_xxl` and has no reason to grow.

The block heights are a budget, not decoration: usable height is 480 − 50 for the footer = 430, and the
wet-day stack is 424 of it. Changing one height means finding the pixels somewhere else. `tools/snapshot.py`
renders what the YAML says, which for an HA-driven page is a screen full of `--`; `tools/preview.py` injects
simulated values from a scenario in `tools/previews/` and is how the wet case was checked. It edits a COPY
and restores the page in a `finally` — verify with md5 either side, because the file it edits is one that
gets flashed.

## Features (`features/`)

A feature is an opt-in package: behaviour a panel may or may not want, kept out of `devices/` (hardware
only) and `layouts/` (pages only). Each one lists what it needs from the other two in its header — stable
ids like `backlight`, `main_touchscreen`, `main_lvgl`, `go_home`, `home_btn` — and **must be listed after
`layout:`**, so any page it brings appends after `splash`. The generic examples carry the opt-in lines
commented out; `home35.yaml` and `sdl-home.yaml` use them.

**Settings are Home Assistant controls with YAML defaults** (upstream #53: Ryan wants HA control *and* the
YAML way kept). Every tunable is a template `number` / `switch` / `select` with `restore_value`, whose
`initial_value` is a substitution the feature defaults and a top-level config can override. So a config
sets a panel's starting point, and HA changes a running one without a reflash.

### `features/idle/` — dim, go home, sleep, wake

`idle.yaml` is the base. After **Dim after** minutes it dims to **Dim level** % of the ceiling, after **Go
home after** it runs the layout's `go_home`, after **Sleep after** it sleeps; `0` means never
(`idle_ms()` in `idle.h` maps it to a threshold the inactivity time cannot pass — `on_idle` timeouts are
templatable). A touch wakes the backlight to the ceiling via `!extend main_touchscreen` → `on_touch`.

What is load-bearing:

- The dim leaves the screen **fully touch-live**. That is why the layout still dismisses `confirm_box` at a
  fixed 5 minutes: a forgotten dialog would otherwise leave a live Turn Off under the next finger.
- The sleep is safe to wake, because `resume_on_input` defaults true (`lvgl/__init__.py`), so the tap that
  wakes a dark screen is swallowed and never actuates what it landed on.
- **`display_sleep` waits for the finger to lift before `lvgl.pause`.** `resume_on_input` wakes on a
  *release* (`LVTouchListener::release()` → `maybe_wakeup()`), so a pause under a held finger — the Home
  hold, below — would undo itself on lift, and the next tap would land on a live screen.
- **Go home is skipped while paused**, so it can share a timeout with the sleep without a page change
  racing `lvgl.pause` — the old "go home at 20, sleep at 30" spacing was that race's only guard, and the
  timeouts are now user-set. It is also skipped while `idle_hold_page` is set.
- **The dim only ever lowers the backlight.** A sleep clock put up by hand sits below the dim level; the
  timers keep counting from the hold, and without this the clock would be dimmed *up*.

**Sleeping** — the timer, holding the footer's Home button for 1.5s, and the **Sleep now** HA button — all
run `sleep_now`. It offers the sleep to `sleep_handler` first, a `std::function` global another feature
may register; if none does, or it declines, the panel goes dark. That is how the sleep clock takes over
without `idle` knowing it exists. Home's own `on_press` still goes home at touch-down, so a hold means go
home, then sleep. The hold is attached in C++ from `on_boot`, not in `footer/widget.yaml`, because the
footer is shared by every layout whether or not it idles. **Sleep last event** records each sleep and
touch-wake with its time ("clock (idle) 02:13", "dark (HA) 23:40"); it reads `unknown` after a reboot,
which is itself the tell.

### `features/idle/sun.yaml` / `ambient_light.yaml` — the ceiling

Both set `active_brightness` (the ceiling everything dims relative to) and `ambient_night` in three bands,
day 100 / dusk 60 / night 35 %, re-applying only on a change of band. `sun.yaml` bands `sun.sun`'s
elevation at ±6° and needs HA's sun integration; its `isnan` guard matters, because an unavailable sensor
publishes NAN, and NAN compared lands you in the night band at noon. `ambient_light.yaml` is for a board
with a light sensor: it `!extend`s a sensor the device file must name `ambient_light`, in lux. The Guition
has none, so home35 uses `sun.yaml`; the CYD has an LDR on GPIO34 but its device file does not expose it
yet, so **`ambient_light.yaml` has only ever been validated, never run.** Without either, the ceiling
stays 100% and nothing is ever night.

### `features/sleep_clock/` — a split-flap clock instead of the dark sleep

With the **Sleep clock** switch on, `sleep_handler` puts a dim clock up instead of going dark — from the
timer, the Home hold or Sleep now alike. It brings its own `skip: true` page, sets `idle_hold_page` while
showing so go-home leaves it alone, and hides the header and footer (`titlebar` / `navbar` on
`top_layer`). A touch returns to the page it replaced, calling `lv_indev_wait_release()` so the release
lands on nothing. **Sleep clock brightness** (1–50 % of the ceiling) applies live while showing. Card and
digit sizes are substitutions defaulting to a 320px canvas.

`flip_clock.h` animates the cards and **finds a card's parts by child index**, so `card.yaml`'s child order
is a contract. The cards carry a 1px white `outline`, not a `border`: a border insets the content box,
moving the halves relative to the full-height face inside them and splitting the digit a pixel off the
hinge; an outline draws outside and changes nothing, but needs a pixel of room, since a parent clips its
children's outlines — hence `pad_all: 1` on the pair objs.

**Night colours** (At night / Always / Never) turns the whole face red — digits, card tint, outline, date
and AM/PM — or the outline and date would be the bluest, brightest things left. "At night" reads
`ambient_night`, so it follows whichever ceiling feature is in use, and never fires without one. The
palette is restyled only on a change (`sleep_clock_red`), because restyling invalidates the whole face.
Night ink is 4.1:1 on its card; outline 4.5:1 and date 3.4:1 on black. Brightness matters more than
colour: IPS black leaks backlight, so the 10% slider does most of the work.

**24-hour time** is a separate switch in `layouts/widgets/header/sensors.yaml`, so every layout exposes it;
it drives the header clock, the sleep clock and the printer end times (the tiles on their next
remaining-time update).

**Never render a transformed object at scale 0.** In LVGL 9.5, `lv_obj_refr()` creates a layer for any
object with a transform, and `lv_draw_layer()` returns early for `scale <= 0` *without* queueing the
blend task that frees it. So every frame drawing a visible obj at scale 0 leaks one layer (~24KB). The
first version of the flip made the lower flap visible at scale 0 for its 160ms delay. That leaked a few
layers per flip, every minute, until PSRAM ran out a few hours into the night. The panel then rebooted
to `printers`, which looked like a crash in "deeper sleep". `flip::scale_cb` now hides a flap at 0.
LVGL fixed it in 9.6.0 (commit `3fff79153`, "skip obj refr for zero scaled") and did not backport it
to 9.5; reported to ESPHome as esphome/esphome#19439. Once ESPHome pins LVGL 9.6 or later, the hiding is
no longer needed, though it does no harm. Found in SDL: `heap <pid>` showed thousands of live 24KB
blocks, and `MallocStackLogging=1` plus `malloc_history <pid> -allBySize` traced them to
`lv_draw_layer_create`. `leaks` did *not* catch it, because the layers stay linked on the display's layer
list and so remain reachable.

### `features/diagnostics/` — and what stays in `common.yaml`

**Uptime** and **Reset Reason** are always on, in `common.yaml`: the evidence for an unexplained restart
can only be caught at the boot that follows it, which is exactly when an opt-in would not have been on —
the panel rebooted twice before these existed, and why was lost. The memory sensors are opt-in:
`memory.yaml` (Heap Free / Min Free / Largest Block, Loop Time) and `psram.yaml` (PSRAM Free, only for a
board whose device file configures `psram:`). They report every minute, which is a recorder row a minute
each — turn them on for the panel you are chasing a problem on. A leak shows as Free or Min Free trending
down over hours; Largest Block falling while Free holds is fragmentation.

### Where recent work lives

- **BLE proxy** — upstream #62, merged into `features/ble_proxy/`. home35 runs it since 2026-09-21 with
  `scan_parameters: active: false`: every proxy in the house scans passively, since nothing here needs
  scan responses (Bermuda, Private BLE Device, Govee H5127). Measured on home35
  (2026-09-21): ~95KB internal RAM while running (the S3 controller cannot use PSRAM; `use_psram` saved
  11KB), ~400KB flash, ~2% of a core. Interleaved on/off pings: no extra loss, p99 1.0s -> 2.5s with a
  30ms/320ms scan window, worse with ESPHome's default continuous scan. Doesn't fit the CYD (app
  partition overflows by 69KB). Its HA switch must be re-applied from `on_boot` at priority 300: the
  template switch restores at setup 798, before esp32_ble (350), and an early `ble.enable` is dropped.
  Enabling/disabling blocks the loop ~210ms; the heap stays fragmented after disable until reboot.
- **Guition JC1060P470C (ESP32-P4, 7" 1024x600)** — `explore/p4-jc1060p470`, unverified. Base file uses
  ESPHome's own `JC1060P470` model; `-V2` for the 2026 panel (V2 on the rear label: different init, reset
  GPIO0, SDIO 10MHz). Needs `engineering_sample: true`. No 1024x600 layout yet.

### Testing features in SDL

`sdl-home.yaml` opts into the same features as home35. To run the idle ladder in seconds, publish
fractional minutes straight to the numbers — `id(idle_sleep_minutes).publish_state(15.0f / 60)` —
since the HA controls take whole minutes. `tools/snapshot.py <sdl config>` writes a PNG of every page
(it wraps the config with `-DLV_USE_SNAPSHOT=1` and an `on_boot` that shows each page, hides the boot
screen and snapshots the screen plus `lv_layer_top()`); a raw `lv_snapshot_take()` misses the header and
footer, which live on the top layer, and shows only the boot screen until something hides it. Name any
helper namespace other than `snapshot` -- ESPHome has an `esphome::snapshot` that makes it ambiguous. To
see the clock itself, snapshot while it shows. The host build ignores `set_epoch_time()`, because `settimeofday` fails there and the host clock
wins, so to catch a flip call `flip::set()` on a card directly. A second LVGL pointer created in an
`on_boot` lambda (`lv_indev_create()` + a scripted `read_cb`) drives touches deterministically; its
first gesture after boot is dropped, so lead with a throwaway tap.

## Checks that pay for themselves

The tools in `tools/` are the checks; run them rather than re-deriving them. `lint_configs.py` covers the
two failures nothing else catches — an undefined `${var}` (the tile subscribes to the literal string) and
an icon missing from the MDI subset (it renders blank) — plus, since #61, a page that forgot its
`page_access.yaml` include. `check_entities.py home35.yaml` catches the third: an `entity_id` that no
longer exists in Home Assistant. It reads every state in one call through the SSH add-on's
`SUPERVISOR_TOKEN` (`ssh homeassistant`; or `HA_URL` + `HA_TOKEN`), and counts absent AMS slots separately,
since which ones are absent *is* the real topology. 104 entities, 0 missing on home35 on 2026-09-23.

**`esphome config` does not compile lambdas.** It resolves and validates the YAML, which is what makes it
the fastest feedback loop — but a lambda that does not build passes it untouched. A page whose C++ was
rewritten needs `esphome compile` before it can be called working; `tools/flash.sh` compiles before it
uploads, which is the other reason to use it rather than a hand-chained upload.

One-off commands still worth knowing:

```bash
git log main..upstream/main                 # empty => everything upstream merged is integrated
gh pr list --repo RyanEwen/esphome-lvgl --state open --author pleasantone
esphome config home35.yaml > /tmp/cfg.txt   # then grep the resolved config, never read it inline
```

Ryan runs no GitHub Actions (no `.github/` upstream); #70 offers to run the lint on PRs if he wants it.

**The MDI subset is split so merges never touch it:** `glyphs.yaml` stays byte-identical to upstream's, and
the personal icons live in `glyphs-home.yaml`, which `fonts/glyphs-home.package.yaml` appends to all five
MDI sizes with `!extend` (ESPHome appends to a font's `glyphs:` list rather than replacing it: 35 + 34 = 69
per size on home35, 35 on the examples). Before the split, the fork's additions to the shared file
conflicted on every merge, and one resolution left five duplicates that failed every config.

**Never `git add -A` in this repo on a branch cut from upstream.** Upstream carries no `.gitignore`, so
that stages the whole `.esphome/` build tree and `secrets.yaml` — the WiFi password and the API key. Add
paths explicitly. `esphome compile` also writes its own stock `.gitignore` into any config directory that
lacks one, which is where a stray untracked copy comes from.

## Boot-time page selection (gotcha)

Every layout goes home from `esphome: on_boot` at priority −100, not from a page. **Do not move it back
into `splash`'s `on_load`.** `lvgl.page.show` called synchronously from inside the initial page's own load
event is dropped, leaving the empty first page active — which renders white, with the header and footer
still drawn because they live on `top_layer`. The symptom looks like a rendering or data problem, but
pressing the home button (the same action, later) fixes it, which is the tell. `splash` used to dodge
this with a 250ms `delay:`, but it only worked while `splash` was the first page, which a package listed
before `layout:` can change. `on_boot` at −100 runs after LVGL has shown whatever page is first, so it
does not care which that is — verified in SDL with a package page deliberately placed first. `splash`
stays as a blank `skip: true` page 0, covered by `top_layer`'s boot screen for the instant before.

**Write every `on_boot` as a list** (`on_boot:` → `- priority: … then: …`), in every file. Packages merge
two dict-form `on_boot`s into *one* automation, keeping the last file's priority for all of them, so a
feature at priority 0 lands after the layout's −100 `go_home` instead of before it. And a list-form
`on_boot` merged after a dict-form one *replaces* it, silently dropping the layout's `go_home`. Lists
concatenate and each keeps its own priority. Found on 2026-09-21, when `features/page_access/` needed its
pages registered (priority 0) before `go_home` ran; until then every file used −100, which hid it.

## The ILI9342 CYD and `320x240-home` (home28) — 2026-09-21

`devices/ESP32-2432S028-9342.yaml` is the USB-C + Micro-USB CYD revision (ILI9342, natively landscape
320x240), verified on hardware: ESPHome's own `mipi_spi` model `ESP32-2432S028-9342` with
`color_order: RGB` (the model's default BGR swaps red and blue), `lvgl: rotation: 270` for the portrait
canvas, and `swap_xy` as the *only* touch transform. LVGL rotates touch points itself
(`lvgl_esphome.cpp` `rotate_coordinates()` on every read), so a touch/display mismatch is a transform
problem and rotation never fixes it. `home28.yaml` runs the personal layout on it; the board normally
runs Paul's other project (`~/ESPHome-touch-display-mount/.../cyd-2432s028-ili9342/home-like.yaml`) as
`smartdisplay`, sharing this repo's API key, so HA reuses one entry keyed by MAC for either firmware.

`layouts/320x240-home/pages/` are **symlinks** into `480x320-home/pages/` — an include resolves from the
link's own path, so each page picks up `320x240-home/vars/`. `printers.yaml` is the one real file.

**No PSRAM is the whole story on this board** (180KB DRAM, 4MB flash):

| build | static RAM | HA subscriptions | result |
|---|---|---|---|
| all pages, 12 AMS slots, sleep clock, 25% buffer | 48.4% | 104 | crash in setup (mDNS task, then abort) |
| no printers, no sleep clock | 41.2% | 64 | 95KB free, 86KB min |
| + printers (12 slots) | — | — | LVGL "Failed to allocate", task_wdt |
| printers enumerated (3 H2C units) + clock, 25% | 46.3% | 104 | abort growing the HA subscription vector |
| same, `buffer_size: 12%` | 46.3% | 104 | **boots; 73KB free, 63KB min, loop 46ms** |

The 25% draw buffer is one 38KB block taken before the subscriptions grow into what is left; 12% was
the fix. Flash is 90% (the OTA partition is 1.79MB). Other fits: `text_sm: 12` so "100%" fits a 31px
tray pill, `text_md: 16` so "TV Backlight" fits a 114px tile, containers scroll (four pages have a row
more than 240px of height), and the confirm box is 228px wide. Debug serial on this board: a data cable
shows `/dev/cu.usbserial-*`; pulse RTS to reset and read the boot (esphome logs over serial doesn't).

Upstream #67 (merged) carries the device file, built on `ESP32-2432S028R.yaml` and `!remove`-ing its
display, with `sunton-28-9342-example.yaml`. Upstream #68 is the diet write-up: a "Putting a low-memory
panel on a diet" README section with the measured table, and `320x240/pages/printers-on-a-diet.yaml`
(real AMS units, commented out in the 2.8" examples). With page access on `320x240-home`, home28 is at
49.0% static RAM (was 46.3%) and has not been flashed since; watch Heap Min Free if it is.

## Per-device overrides from a top-level config

Because packages are merged, a top-level or device file can reach into the layout with `!extend`:

```yaml
lvgl:
  pages:
    - id: !extend bedroom
      skip: true          # hide a page on this device only
```

`display:` and `touchscreen:` in every device file carry `id: main_display` / `id: main_touchscreen` for the
same reason — so rotation or transforms can be extended from above.

## One image on several panels

Upstream #61 makes the home page and page visibility Home Assistant controls on every panel, generic and
personal layouts alike: a **Home page** select plus a **Show \<page\> page** switch per
page, all from `layouts/widgets/page_access.yaml`, one include per page. `skip: true` in YAML still wins —
the switch leaves such a page alone. The select does **not** restore itself: a template select saves
its *index*, which points at another page once the page list changes, so the layout keeps a restored
`home_page_hash` (fnv1 of the name) and each page's `page_access.yaml` puts the select back at boot
priority 0, before `go_home` at −100. With those, a room differs from another only in runtime settings, so
one image can serve several panels: `esphome: name_add_mac_suffix: true` in the top-level config makes
each join Home Assistant as its own device (`<name>-<last 3 MAC bytes>`). That line is the whole opt-in;
per-panel configs just don't set it.

Updating such a fleet is a loop, which keeps `common.yaml`'s encrypted OTA — the suffixed names mean
`esphome upload` can't find a panel by name:

```bash
esphome compile panels.yaml
for ip in 192.168.89.135 192.168.89.140; do esphome upload panels.yaml --device $ip; done
```

Deliberately not built (scoped 2026-09-21, ~half a day): updates from Home Assistant via
`update: platform: http_request`, a manifest on HA's `/config/www`, and a publish script (ESPHome has no
manifest generator; the image is `build/firmware.ota.bin`, and the version must come from `esphome:
project:`, bumped per build with `-s`). It would give each panel an Install button, but the download is
authenticated only by an md5 in the same unauthenticated manifest, which undoes encrypted OTA for anyone on
the LAN, and it costs ~40–60KB of flash plus a buffer on every panel — tight on the CYD. Build it only if
someone running several panels asks.

## Home Assistant coupling

Buttons call HA via `homeassistant.action:` (the modern name for services), which requires "Allow the device
to perform Home Assistant actions" to be enabled for the ESPHome device in HA. State flows the other way
through `platform: homeassistant` sensors in the `*sensors.yaml` files.
The API is encryption-only (`api: encryption:` in `common.yaml`, key from `secrets.yaml`), and `ota:`
carries a bare `encryption:` block that inherits that same key, so plaintext OTA is refused. The clock comes from
`time: platform: homeassistant`, and `common.yaml` captures the HA IP into the `homeassistant_ip` global on
API connect. The `entity_id`s throughout `layouts/` are the author's own (lights, TVs, Bambu printers)
and are meant to be edited.

The four lighting pages (`lighting_main`, `lighting_second`, `lighting_ground`, `lighting_outside`,
in that order) lean on helpers that live only in Home Assistant, not in this repo — the `light` group
helpers `main_floor_lights` / `second_floor_lights` / `outside_lights`, and a `switch_as_x` turning
`switch.porch_light` into `light.back_yard_light`. The group buttons are stateful, so they need a real
entity to read; targeting `floor_id:` in the action would toggle but never light up. Front Porch sits in
Home Assistant's Main Floor area and is shown on the Outside page anyway — a display choice, not an area
change.

The Living Room page's Movie Time button fires `scene.movie_time`, which lives only in Home Assistant.
Deleting or renaming that scene there silently breaks the button — `homeassistant.action` has nothing to
validate against at build time.

The Second Floor page's Good Night button (home35's home page) runs `script.good_night`, also HA-only.
It works out its targets at run time from **areas assigned to a floor**, so floorless areas (printers,
SpoolBuddy, the car) are never touched: it pauses any media player there that is playing, powers off
those labelled `good_night_power_off` (LG TV, Apple TV, Denon receiver), and turns off every light not
labelled `not_room_lighting` (panel backlights). home35 sits in the Bedroom area, which is why its
backlight carries that label. Change what it does with labels and areas in HA, not by editing the script.

Brightness crosses this boundary in two units. **Author in percent everywhere; raw 0-255 only where
Home Assistant forces it**, which is scene storage — a scene holds attributes, and there is no
`brightness_pct` attribute for it to hold, so `scene.movie_time` keeps `brightness: 38`. Automations
gate on `sensor.living_room_chandelier_brightness`, a template sensor exposing percent, rather than on
the raw `brightness` attribute, so a threshold and an action can no longer disagree about the unit.
The panel's own raw-to-percent conversion lives only in `light_buttons/dimmable.yaml` and its sensors.

Upstream's `bedroom` page is dropped from the personal layouts — its lights are already on the Second Floor
page and none of its media entities exist in this house. The personal `living_room` is rebuilt around the
real lights, the Movie Time button and a temperature tile. `widgets/bedroom_tv.sensor.yaml` and
`widgets/living_room_tv.sensor.yaml` stay for the generic pages that include them.

`light.living_room_chandelier` appears on both the Main Floor and Living Room pages, with its own `uid`
on each. That is deliberate, and it is why widget ids derive from `uid` rather than from `entity_id`.

## Conventions worth preserving

- `common.yaml` pins `esphome: min_version:`; bump it when a config starts depending on newer components.
- Rotation for an LVGL device goes in a device-level `lvgl: rotation:` block, never `display: rotation:` —
  ESPHome rejects the latter outright when LVGL drives the display. `devices/ESP32-2432S028R.yaml` uses the
  same device-level `lvgl:` override pattern for `buffer_size`.
- Displays use `update_interval: never` and `auto_clear_enabled: false` — LVGL drives the refresh.
- Device sections are written as lists (`display:` `- id: ...`) rather than a mix of mapping/list forms.
- Touch-calibration `on_touch:` lambdas are left commented out in each device file; uncomment to log raw
  coordinates when adding or fixing a board.
- The README carries a dated changelog; note breaking changes there when altering the package contract.
- **Measure text, do not estimate it** — PIL against the cached font in `.esphome/font/` gives the width
  and the line height. It is how the AMS columns were sized and how the Outside page's verdict strings,
  block heights and font sizes were chosen. Guessing costs a flash cycle to find out.
- **A hidden widget must not be able to mean two things.** If a block hides when a value is absent and also
  when the value is benign, the panel is lying on one of those days. Either show a statement of the benign
  case, or keep the block and say what is missing.
