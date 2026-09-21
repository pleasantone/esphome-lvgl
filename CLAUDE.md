# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A pure-YAML ESPHome configuration repo (no code, no build system of its own) that drives LVGL touchscreen
dashboards on cheap ESP32 display boards, backed by Home Assistant. Every file is ESPHome YAML consumed
through the `packages:` / `!include` mechanism.

## Commands

```bash
esphome config <top-level>.yaml     # validate + dump the fully-substituted config (fastest feedback loop)
esphome compile <top-level>.yaml    # compile firmware
esphome run <top-level>.yaml        # compile + upload (USB or OTA)
esphome run sdl-example.yaml        # run the UI on the host in an SDL window — no hardware needed
esphome logs <top-level>.yaml
```

Only the top-level files in the repo root (`*-example.yaml`) are directly buildable; everything under
`devices/` and `layouts/` is a package fragment that fails on its own.

`esphome config` is the primary debugging tool here: because the layouts are built from anchors, merge keys
and parameterized includes, the resolved output is the only reliable way to see what a widget actually
becomes.

Non-SDL builds need a `secrets.yaml` next to the config with `wifi_ssid` and `wifi_password` (not in the
repo). `sdl-example.yaml` deliberately skips `common.yaml`, so it needs no secrets and no WiFi.

## Composition model

A top-level config is only a few lines — it names the device, picks a home page, and merges three packages:

```yaml
esphome: { name: ..., friendly_name: ... }   # device_name/friendly_name live HERE, not in substitutions
substitutions: { home_page: lighting_1 }
packages:
  common: !include common.yaml               # WiFi, API, OTA, web_server, diagnostic sensors
  device: !include devices/<BOARD>.yaml      # pins, display, touchscreen, backlight, psram, framework
  layout: !include layouts/<WxH>.yaml        # fonts, theme, LVGL pages, and all HA sensors
                                             # (or <WxH>-home.yaml -- see "Personal vs example layouts")
```

**A layout's name is the canvas it was drawn for, not what every board gives it.** The Guition
`JC3248W535` is natively **320x480 portrait** (see `width=320, height=480` in ESPHome's
`mipi_spi/models/jc.py`) and this repo sets no rotation for it, so the `480x320` layouts are actually
driven at 320px wide on that board — about 298px usable inside the page and tile padding. Budget widths
against the real canvas, not the filename. Boards that need landscape set `lvgl: rotation:` in their
device file.

The three axes are intentionally orthogonal: `devices/` files contain *nothing* UI-related, and `layouts/`
files contain nothing board-specific. Layouts are keyed by resolution (`320x240`, `480x320`, `800x480`), so
several boards share one layout. Adding a board means adding one file to `devices/` and nothing else.

`home_page` is a required substitution consumed by the `go_home` script in every layout; it is also what the
footer home button triggers.

### Personal vs example layouts

`layouts/<WxH>.yaml` is the **generic example**, kept as close to upstream as the widget set allows: demo
entities (`light.kitchen_light`, printers Fred/Wilma/Barney on `p1s_N`) that no real house has. It is what
every `*-example.yaml` builds, and what can be offered upstream.

`layouts/<WxH>-home.yaml` is the **personal** one -- this author's real lights, alarm, covers and Bambu
serials. Only `home35.yaml` and `sdl-home.yaml` use it.

The split exists because the personal layout had overwritten the example: for a while `layouts/480x320.yaml`
*was* this house, so anyone cloning the repo and flashing `guition-35-example` got a panel wired to entities
they do not own, and `480x320`'s example content survived only in upstream's history. **Put personal
entities in a `-home` layout; leave `layouts/<WxH>.yaml` generic.**

The generic layouts keep upstream's page names (`lighting_1`/`_2`/`_3`), so the examples keep
`home_page: lighting_1`. The personal `480x320-home.yaml` renamed them to `lighting_main` / `lighting_second`
/ `lighting_ground` / `lighting_outside`, which is why `home35.yaml` and `sdl-home.yaml` differ there.

The generic layouts do not carry the old `widgets/printers/widget.yaml`; their printers pages were
recomposed onto `tile_combined.yaml` + `printer_combined.sensors.yaml`, which reproduces upstream's
single-AMS combined line using the current widget set.

## Layout file anatomy (`layouts/<WxH>.yaml`)

Each layout file is the single place where resolution-dependent numbers live, and follows a fixed shape:

1. `font: !include fonts/fonts.yaml` with `vars:` giving the pixel sizes for the `roboto_*` and `mdi_*`
   font ladders (sm/md/lg/xl/xxl). The MDI font is downloaded from Pictogrammers and subset by
   `layouts/fonts/glyphs.yaml` — **any new icon must be added to `glyphs.yaml` or it renders blank.**
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

An on tile is distinguished by its glyph, not by an edge or a colour. A gold 4px bar down the left of
every lit tile was tried and removed (tag `gold-edge`, `git revert` the commit after it to bring it back).
Two things sank it: it cost 4px of content width in *every* state, because LVGL's
`lv_obj_get_style_space_left()` adds `border_width` to the content inset whenever `border_side` includes
`LEFT` whether or not the border is painted — so the width has to be reserved permanently or the text
shifts on every toggle — and the tiles here already carry state by shape, since each one passes a real
`icon_on` / `icon_off` pair.

That shape cue is what makes the colour redundant, and it is worth knowing why: gold `0xFFD700` against
white `0xFFFFFF` is **1.40:1**, well under the 3:1 floor for a non-text indicator. A tile that used one
glyph for both states would be carrying its state on that 1.40:1 alone, which is the case the edge was
built for. None do any more. If a same-glyph tile ever appears here, give it an `icon_off` before
reaching for colour.

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

`layouts/480x320-home.yaml` therefore composes a tile from `&printer_tile` + `&printer_tile_layout` and hands
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

`layouts/480x320-home.yaml` does not enumerate the units a printer actually has. **Each printer carries all
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

`layouts/800x480-home.yaml` still enumerates its units the old way: it names the real X1C and H2C and lists
the four rows they actually have. That file is the personalised one; `layouts/800x480.yaml` is back to
upstream's six demo printers, recomposed onto the current widgets. Enumerating
is the right call there anyway on a 480x800 canvas showing two printers side by side. Upstream branches
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

## Idle behaviour is split across two files on purpose

`devices/JC3248W535.yaml` owns the backlight: dim to 25% of `active_brightness` at 5 minutes, backlight
off plus `lvgl.pause` at 30. `layouts/480x320-home.yaml` owns the UI half in its own `on_idle` — dismiss
`confirm_box` at 5 minutes, `go_home` at 20 — because those are layout ids and a device file must stay
UI-free. **Both lists merge**: packages concatenate them and every entry keeps its own timeout, so adding
one in either file leaves the other alone.

Three things about that arrangement are load-bearing:

- The 5-minute dim leaves the screen **fully touch-live**. That is why the confirm dialog is dismissed
  there: a forgotten dialog would otherwise leave a live Turn Off under the next finger.
- The 30-minute sleep is safe to wake, because `resume_on_input` defaults true (`lvgl/__init__.py`), so
  the tap that wakes a dark screen is swallowed and never actuates what it landed on.
- `go_home` sits at 20 minutes, not 30, so it cannot race the device file's `lvgl.pause`. Ordering
  between two entries with the same timeout is not yours to control, and a page change is not worth
  betting on across a paused LVGL.

`active_brightness` is a ceiling driven by `sun.sun` elevation (day 1.0 / dusk 0.6 / night 0.35). The
panel has no ambient light sensor — the CYD has an LDR on GPIO34, the Guition does not.

## Checks that pay for themselves

Each of these replaced a pile of exploratory calls at least once, and each is verified:

```bash
git log main..upstream/main                 # empty => everything upstream merged is integrated
gh pr list --repo RyanEwen/esphome-lvgl --state open --author pleasantone
esphome config home35.yaml > /tmp/cfg.txt   # then grep the resolved config, never read it inline
```

To check every entity the panel subscribes to actually exists in Home Assistant — the one failure the
build cannot catch — pull them out of the resolved config and ask HA in one call:

```bash
grep -oE 'entity_id: [a-z_]+\.[a-z0-9_]+' /tmp/cfg.txt | awk '{print $2}' | sort -u
```

then `ha_get_state` with that list (max 100 per call; the AMS slots that do not exist are expected to
fail, and which ones fail tells you the real topology).

To audit the MDI subset, compare the codepoints in the resolved config against `layouts/fonts/glyphs.yaml`:
anything used but not listed renders as a box, anything listed but unused is flash spent on nothing. Both
sets were exactly 53 as of 2026-09-19.

**Never `git add -A` in this repo on a branch cut from upstream.** Upstream carries no `.gitignore`, so
that stages the whole `.esphome/` build tree and `secrets.yaml` — the WiFi password and the API key. Add
paths explicitly. `esphome compile` also writes its own stock `.gitignore` into any config directory that
lacks one, which is where a stray untracked copy comes from.

## Boot-time page selection (gotcha)

The `splash` page's `on_load` fires `go_home` behind a `delay:`. **Do not remove that delay.**
`lvgl.page.show` called synchronously from inside the initial page's own load event is dropped, leaving the
empty `splash` page active — which renders white, with the header and footer still drawn because they live
on `top_layer`. The symptom looks like a rendering or data problem, but pressing the home button (the same
action, later) fixes it, which is the tell.

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

Brightness crosses this boundary in two units. **Author in percent everywhere; raw 0-255 only where
Home Assistant forces it**, which is scene storage — a scene holds attributes, and there is no
`brightness_pct` attribute for it to hold, so `scene.movie_time` keeps `brightness: 38`. Automations
gate on `sensor.living_room_chandelier_brightness`, a template sensor exposing percent, rather than on
the raw `brightness` attribute, so a threshold and an action can no longer disagree about the unit.
The panel's own raw-to-percent conversion lives only in `light_buttons/dimmable.yaml` and its sensors.

Upstream's `bedroom` page is dropped here — its lights are already on the Second Floor page and none
of its media entities exist in this house. `living_room` is rebuilt around the real lights, the
`scene.watching_tv` button and a temperature tile. `widgets/bedroom_tv.sensor.yaml` and
`widgets/living_room_tv.sensor.yaml` stay because `layouts/320x240.yaml` still includes them.

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
