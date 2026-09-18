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
```

**A layout's name is the canvas it was drawn for, not what every board gives it.** The Guition
`JC3248W535` is natively **320x480 portrait** (see `width=320, height=480` in ESPHome's
`mipi_spi/models/jc.py`) and this repo sets no rotation for it, so `layouts/480x320.yaml` is actually
driven at 320px wide on that board — about 298px usable inside the page and tile padding. Budget widths
against the real canvas, not the filename. Boards that need landscape set `lvgl: rotation:` in their
device file.

The three axes are intentionally orthogonal: `devices/` files contain *nothing* UI-related, and `layouts/`
files contain nothing board-specific. Layouts are keyed by resolution (`320x240`, `480x320`, `800x480`), so
several boards share one layout. Adding a board means adding one file to `devices/` and nothing else.

`home_page` is a required substitution consumed by the `go_home` script in every layout; it is also what the
footer home button triggers.

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

`widgets/status/` holds tiles that report without controlling. They plug in under `obj:` rather than
`button:`, which the theme renders darker (`0x42495A` vs `0x5A6173`) so they read as not pressable.
`status/binary/` reuses the buttons' `stateful_sensors.yaml` and takes `icon_on` / `icon_off` /
`color_on`; `status/alarm/` maps an `alarm_control_panel` state string to a label and recolours its
shield.

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

There is deliberately **no per-topology tile file**. A printer tile is composed in
`layouts/<WxH>.yaml` from `&printer_tile` + `&printer_tile_layout`, a name label, one `ams_row*` include
per AMS unit, then status and progress. ESPHome YAML has no loops, so a tile file would have to hardcode
a unit count and fork for every combination (1 AMS, 2 AMS, 2 AMS + HT, 2 HT…). Adding a second AMS HT is
one more row include plus its three sensor packages — nothing else changes.

Every tray id is `${uid}_ams_${ams_id}_tray_N_*`, so a unit is identified purely by `uid` + `ams_id`.

Nothing in an AMS row is ever hidden and every column has a fixed width: hiding a child removes it from
the flex row and shifts the pill columns out of alignment.

Right-justification is done by giving each column its own container with `flex_align_main: END`.
**Do not use `text_align` for this** — it is a valid property but was observed not to take effect here,
which is what left a wide gap between the humidity value and its glyph.

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

`bedroom` and `living_room` are upstream's pages and reference entities that do not exist here
(`light.bedroom_light_1`, `media_player.ryan_s_xbox`, `script.play_content_on_bedroom_chromecast`, …).
They are knowingly left broken.

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
