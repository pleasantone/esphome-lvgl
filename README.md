# ESPHome + LVGL on cheap touchscreen devices

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/S6S21K2LK2)

## Supported Devices
* Guition `JC3248W535` 3.5" 320x480 portrait, with capacitive touch and USB-C. [AliExpress Link](https://www.aliexpress.com/item/1005007566046827.html).
* Sunton `ESP32-2432S028R` 2.8" 240x320 portrait, with resistive touch and USB micro-B. [AliExpress Link](https://www.aliexpress.com/item/1005004502250619.html).
* Sunton `ESP32-8048S043` 4.3" 480x800 portrait, with capactivive touch and USB-C. [AliExpress Link](https://www.aliexpress.com/item/1005004788147691.html).
* Sunton `ESP32-8048S050` 5.0" 480x800 portrait, with capactivive touch and USB-C. [AliExpress Link](https://www.aliexpress.com/item/1005004952694042.html).
* Elecrow CrowPanel `DIS05035H` (v2.2) 3.5" 320x480 portrait, with resistive touch and USB-C. [Manufacturer's Link](https://www.elecrow.com/esp32-display-3-5-inch-hmi-display-spi-tft-lcd-touch-screen.html).

## Changelog
### 2026-09-20
* [Breaking change] Personal and example layouts are now separate files. `layouts/480x320.yaml` and `layouts/800x480.yaml` are back to upstream's generic demo content (`light.kitchen_light`, printers Fred/Wilma/Barney), and the personalised versions moved to `layouts/480x320-home.yaml` and `layouts/800x480-home.yaml`. Only `home35.yaml` and the new `sdl-home.yaml` use the `-home` files; every `*-example.yaml` builds the generic ones again. If you had been building an example config and getting someone else's entities, that is what this fixes.
* The generic layouts' printers pages were recomposed onto `widgets/printers/tile_combined.yaml` + `printer_combined.sensors.yaml`, reproducing upstream's single-AMS combined line with the current widget set. The old `widgets/printers/widget.yaml` and `sensors.yaml` remain removed.
* Added `sdl-home.yaml`, so the personal layout can still be previewed in an SDL window; `sdl-example.yaml` now previews the generic one.
* The `480x320` printer page is now composed from `widgets/printers/tile.yaml` (the whole tile body) and one `widgets/printers/printer.sensors.yaml` per printer (core sensors plus all twelve AMS slots), instead of six row includes and 38 sensor includes spelled out per printer. Adding a printer is three lines on the page and four in `packages:`. The resolved config is unchanged, so no reflash is needed; a layout that composes its own tiles inline, as `800x480` does, keeps working untouched.
* The tile body is now a per-printer choice. `widgets/printers/tile.yaml` is the stacked format (a name line, then a self-revealing row per AMS unit); `widgets/printers/tile_combined.yaml` is upstream's original shape, with the printer name and one unit's four pills on a single line. Combined costs one line less but is statically visible and single-AMS, so it suits a machine whose one AMS is always present. Pair each with the matching `printer.sensors.yaml` or `printer_combined.sensors.yaml` — mismatching them fails at config time.
* The four tray pills moved to `widgets/printers/ams_tray_strip.yaml`, shared by the stacked row and the combined line. No visual change: the resolved config for the `800x480` boards is identical.
* The leaf widget and sensor files are otherwise unchanged, and so is the contract that a stateful pair shares a `uid`. What is new is that an `!include` inherits the vars of the file that included it, so the nesting needs no pass-through boilerplate, and that a var can select the include's filename (`ams_row${variant}.sensors.yaml`).
### 2026-09-18
* [Breaking change] The printer tile is now composed in the layout from one AMS row include per AMS unit rather than from a single `widgets/printers/widget.yaml`, so a printer can carry any combination of AMS and AMS HT units. `widgets/printers/widget.yaml` and `widgets/printers/sensors.yaml` are replaced by `ams_row*.yaml`, `tile_status.yaml`, `tile_progress.yaml` and their sensor counterparts. A page that includes the old files needs recomposing; the `printers` page in each layout shows the shape.
* Show per-unit AMS humidity, with a heater icon beside it: amber while that unit is drying, grey while it is merely capable of it, blank where there is no heater.
* AMS rows can optionally reveal themselves as their units report, so a printer's topology does not have to be spelled out in the layout. See "How to let AMS rows appear on their own".
### 2026-09-17
* Document that every supported board draws portrait, and that the files in `layouts/` are named for the panel's nominal landscape resolution rather than for the canvas LVGL draws on. No config changes; the device files were already correct.
* [Breaking change] `common.yaml` now requires an encrypted API and OTA. Add an `api_encryption_key` to your `secrets.yaml` (Home Assistant shows a generated key when adding an ESPHome device, or see the [API docs](https://esphome.io/components/api/)), then reflash each device and enter the same key in Home Assistant. A device that is only reachable over OTA should be flashed before Home Assistant loses the connection to it.
* A bare `encryption:` under the `esphome` OTA platform reuses the API key, so there is no second secret to manage, and its presence is what drops the plaintext OTA fallback (removed from ESPHome after 2027.3.0).
* Update `common.yaml` to require ESPHome min version 2026.9.0, which is the release that added OTA encryption.
### 2024-11-06
* Update `common.yaml` to require ESPHome min version 2024.11.0 (currently in dev) due to upcoming changes to some display and touch drivers, and to add support for the Guition device which uses drivers not supported on the stable release as of yet.
* Update Sunton `ESP32-8048S043` and `ESP32-8048S050` configs to support upcoming ESPHome changes that affect display & touch rotation.
* Add support for Guition `JC3248W535` 3.5" device.
* Tweak devce configs so that majority of sections are in list format rather than a mix of formats.
* Fix touchscreen configs for devices with resistive touch on newer ESPHome builds.
### 2024-11-01
* [Breaking change] Moved `device_name` and `friendly_name` from `substitutions:` to `esphome:` at the top-level. This allows for ESPHome's Rename Hostname feature to work again.
* Added an `id` to each device's `display:` and `touchscreen:` config to allow extending them more easily (for example, if you want to rotate a display / touchscreen from the top-level config for a specific device).

## Orientation and Layout Naming
Every board here draws portrait. The files in `layouts/` are named for the panel's nominal landscape resolution rather than for the canvas LVGL ends up with, so `layouts/480x320.yaml` drives a 320px wide, 480px tall canvas:

| Board | Panel | Canvas LVGL draws on | Layout |
| --- | --- | --- | --- |
| Guition `JC3248W535` | 320x480 | 320x480 | `layouts/480x320.yaml` |
| Elecrow `DIS05035H` | 320x480 | 320x480 | `layouts/480x320.yaml` |
| Sunton `ESP32-2432S028R` | 240x320 | 240x320 | `layouts/320x240.yaml` |
| Sunton `ESP32-8048S043` | 800x480 | 480x800, via `rotation: 90` | `layouts/800x480.yaml` |
| Sunton `ESP32-8048S050` | 800x480 | 480x800, via `rotation: 90` | `layouts/800x480.yaml` |

A layout named `<WxH>.yaml` is the generic example, with demo entities anyone can build. A `<WxH>-home.yaml`
beside it is the repo author's personal version of the same canvas, wired to real entities; only `home35.yaml`
and `sdl-home.yaml` use those. Keep your own entities in a `-home` layout so the examples stay buildable by
other people.

Widget widths in the layouts are percentages, which is what lets one layout serve two different panels. Any size given in pixels has to be budgeted against the canvas width in the table above and not against the layout's filename, which is roughly 150px narrower than the name suggests on the 3.5" boards.

## File Structure
If all you are looking for is a device-specific config then look no further than the `devices/` directory. The YAML files in there are clean and free from anything not related to the devices themselves. They are intended to be used as [Packages](https://esphome.io/components/packages.html) in a higher-level YAML config file, which allows for device-specific settings and common settings to be kept in separate files, avoiding duplicate code and making it easier to update groups of devices. 

The YAML files in the root of this repo demonstrate how to use each device's config file with a common config, as well as a resolution-specific (but not device-specific) LVGL config/layout. 

## Advanced YAML Techniques
Aside from the Packages feature used to separate device-specfic YAML from common YAML config, there are some other potentially unfamiliar techniques in use here. For example, the files within `layouts/` use [YAML anchors and aliases](https://ref.coddy.tech/yaml/yaml-anchors) which help reduce code duplication. I use anchors and aliases instead of `style_definitions` and `styles` as anchors can be used on anything instead of being restricted to just styles, and because they override `theme` settings when used (there is a bug or perhaps odd design choice that prevent `styles` from overriding `theme`). I define most of my anchors within a made-up section called `.sizing` because top-level sections prefixed with a period do not cause errors when parsed by ESPHome. 

## Required Setup in Home Assistant
Don't forget to Configure your ESPHome Devices in Home Assistant, to allow them to perform actions:
![Allow device to perform Home Assistant actions](https://github.com/user-attachments/assets/ca5c3cb4-a4fd-44ea-a5f6-159ccd6401df)

## How-tos
### How to specify the home page on a particular device
To change which page loads at boot time and when the home button is pressed on a particular device, adjust the `home_page` variable in the device's config file to the ID of the desired page. 

For example, to set a page with the ID `printers`, adjust this in your device's config file:
```yaml
substitutions:
  ...
  home_page: printers
```

### How to hide pages on particular devices
To hide a page on a particular device, extend the desired `page` definition by adding `skip: true` using `!extend` (see [Packages](https://esphome.io/components/packages.html) feature). 

For example, to hide a page with the ID `bedroom`, add this to your device's config file:
```yaml
lvgl:
  pages:
    - id: !extend bedroom
      skip: true
```

### How to let AMS rows appear on their own
By default a layout lists the AMS units a printer has, and that list is fixed at compile time. If you would rather not respell it every time a unit moves, include the rows `hidden: true` and let each unit reveal its own row:

```yaml
- obj: # AMS 2 -- hidden until this unit reports humidity
    hidden: true
    <<: !include { file: widgets/printers/ams_row.yaml, vars: {
      uid: printer_1, ams_id: "2", label: "2", <<: *ams_row_vars } }
```

`ams_row_humidity.sensors.yaml` calls `lvgl.widget.show` on the row when a reading arrives. Every AMS reports humidity, so that doubles as "this unit is here": a slot whose entities do not exist never sends anything and stays hidden.

Include a slot for every unit the printer could have — `1` to `4` for AMS units and `128` upwards for AMS HTs — with its sensor packages, and the tile then follows the hardware. Moving an AMS from one printer to another needs no reflash: the old row stops updating and the new one appears. A row hides itself again when its humidity reading goes away, so a unit that is removed does not leave a row of frozen values behind — though an integration that freezes a missing device's entities rather than marking them unavailable sends nothing to hide on.

Subscribe `ams_row_drying.sensors.yaml` for every slot as well and the heater icon becomes discovered rather than declared — a unit with no drying hardware has no `_drying` entity, so its icon simply stays blank.

The cost is the slots you do not use: on a Guition `JC3248W535`, going from 4 enumerated rows to 12 slots across two printers took RAM from 41.2% to 44.0% and flash from 18.7% to 19.3%. Empty slots are silent at boot, since Home Assistant sends nothing at all for an entity that does not exist.

## Todo
This readme isn't finished. I'll be elaborating on some more techniques being used in here, such as the modularization of the widgets using `!include` and how the stateful widget files relate to their sensor counterparts (tip, just make sure to pass the same `uid` and `entity_id` when including a widget and when including the related widget sensor).

## Photos
These look better in real life, I promise! I took these photos in low-light and displays are not easy to photograph in general.

4.3" 480x800 portrait (Sunton ESP32-8048S043)  
![Lighting Page](media/sunton_4.3_lighting.jpg "Lighting Page")
![Printers Page](media/sunton_4.3_printers.jpg "Printers Page")

3.5" 320x480 portrait (Guition JC3248W535)  
![Lighting Page](media/guition_3.5_lighting.jpg "Lighting Page")
![Printers Page](media/guition_3.5_printers_ams.jpg "Printers Page")  

3.5" 320x480 portrait (Elecrow DIS05035H)  
![Lighting Page](media/elecrow_3.5_lighting.jpg "Lighting Page")
![Printers Page](media/elecrow_3.5_printers.jpg "Printers Page")

2.8" 240x320 portrait (Sunton ESP32-2432S028R)  
![Lighting Page](media/sunton_2.8_lighting.jpg "Lighting Page")
![Printers Page](media/sunton_2.8_printers.jpg "Printers Page")
