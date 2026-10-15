#pragma once
// Fork-only: one printer as a "lane" on the square Printers page (4848S040 mockup, 2026-10-15).
//
// Same feed as the cards (sensor.office_panel_printers <p>_page_all, see feed.h), laid out for a
// glance from across the room rather than for completeness:
//   ring      progress, with the % and the layer inside; a state icon when idle / done / failed
//   info      name + status pill, the job, time left with the finish time, the three temperatures
//   alert     the print error, in red
//   rows      one per AMS unit: full-colour swatches with grams, then humidity or drying time
// A printer that is off collapses to its name row, so the other one gets the room.
#include "feed.h"

namespace feed {

struct Lane {
  lv_obj_t *ring_box, *ring, *pct, *sub, *icon;
  lv_obj_t *pill, *pill_lbl, *task;
  lv_obj_t *time_row, *left, *done;
  lv_obj_t *temps, *t_noz, *t_bed, *t_ch;
  lv_obj_t *alert;
  Row rows[3];
};

// "Layer 88 of 210 · 31 g · AMS 1-2" -> "88/210"
inline std::string layer_of(const std::string &line) {
  size_t at = line.find("Layer ");
  if (at == std::string::npos) return "";
  std::string s = line.substr(at + 6);
  size_t dot = s.find(" \xc2\xb7");
  if (dot != std::string::npos) s.resize(dot);
  size_t of = s.find(" of ");
  if (of != std::string::npos) s.replace(of, 4, "/");
  return s;
}

inline void set_lane(const Lane &w, const std::vector<std::string> &f) {
  auto head = split(part(f, 0), '|');
  auto job = split(part(f, 1), '|');
  auto temps = split(part(f, 2), '|');
  std::string state = part(head, 0), status = part(head, 1), task = part(head, 2);
  int pct = atoi(part(job, 0).c_str());
  bool running = state == "active" || state == "paused";
  bool on = state != "off";

  // status pill, as the cards draw it
  uint32_t pb = OFF_BG, pt = SUB;
  if (running) pb = WARN_BG, pt = WARN_TEXT;
  else if (state == "failed") pb = ACT_BG, pt = TEXT;
  else if (state == "done") pb = GREEN_BG, pt = TEXT;
  else if (state == "starting") pb = BLUE_BG, pt = TEXT;
  else if (state == "idle") pb = OFF_BG, pt = TEXT;
  bg(w.pill, pb);
  fg(w.pill_lbl, pt);
  text(w.pill_lbl, status);
  text(w.task, task.empty() ? std::string(on ? "No job" : "") : task);
  show(w.task, !task.empty() || on);

  // the ring: progress while there is any, else an icon for the state
  show(w.ring_box, on);
  int v = running || state == "failed" ? std::max(pct, 0) : state == "done" ? 100 : 0;
  lv_arc_set_value(w.ring, v);
  uint32_t rc = state == "failed" ? ACT_BG : state == "done" ? GREEN : state == "paused" ? DIM : AMBER;
  lv_obj_set_style_arc_color(w.ring, lv_color_hex(rc), LV_PART_INDICATOR);
  bool numeric = running || state == "failed";
  show(w.pct, numeric);
  if (numeric) text(w.pct, std::to_string(std::max(pct, 0)) + "%");
  std::string sub = state == "paused" ? "Paused" : state == "failed" ? "Failed" : layer_of(part(job, 3));
  text(w.sub, sub);
  fg(w.sub, state == "failed" ? RED_TEXT : DIM);
  show(w.sub, numeric && !sub.empty());
  const char *ic = state == "done" ? "\U000F012C" : state == "starting" ? "\U000F0425" : "\U000F042B";
  text(w.icon, ic);
  fg(w.icon, state == "done" ? GREEN : state == "starting" ? 0x7FB0FF : DIM);
  show(w.icon, !numeric);

  // time: left + finish while printing, the finish after, else the auto-off note
  std::string l, r;
  if (running) {
    l = part(job, 1);
    r = part(job, 2);
  } else if (!part(job, 2).empty()) {
    l = part(job, 2);
  } else {
    l = part(f, 3);
  }
  if (!on && l.rfind("Auto-off when on", 0) == 0) l = "";
  text(w.left, l);
  text(w.done, r);
  show(w.done, !r.empty());
  show(w.time_row, !l.empty() || !r.empty());

  // temperatures, only while there are any
  std::string n = part(temps, 0), b = part(temps, 1), c = part(temps, 2);
  bool any = on && (n != "\xe2\x80\x94" || b != "\xe2\x80\x94" || c != "\xe2\x80\x94");
  text(w.t_noz, n);
  text(w.t_bed, b);
  text(w.t_ch, c);
  show(w.temps, any);

  text(w.alert, part(f, 4));
  show(w.alert, !part(f, 4).empty());

  // filament: the shared row drawing, then the unit's whole note -- "16% · 24°", "Dry 2 h 10 m"
  for (int i = 0; i < 3; i++) {
    std::string row = part(f, 5 + i);
    set_row(w.rows[i], row);
    if (row.empty()) continue;
    auto h = split(split(row, ';')[0], '|');
    std::string meta = part(h, 1);
    if (meta.rfind("Drying", 0) == 0) {
      size_t sep = meta.find("\xc2\xb7 ");
      meta = "Dry" + (sep == std::string::npos ? std::string("ing") : " " + meta.substr(sep + 3));
    }
    text(w.rows[i].meta, meta);
  }
}

}  // namespace feed
