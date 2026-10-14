#pragma once
// Fork-only: draws Home Assistant's panel feeds on home35 (2026-10-13).
//
// The office desk panel's feed sensors (~/homeassistant/office-panel, Jinja + template sensors on
// HA) already shape the chip row and both printers into short strings. This panel reads the same
// attributes instead of subscribing to ~100 entities and re-deriving them:
//   sensor.office_panel_status   chips_all     "v␟icon|level|text␟…"  (8 chips, My Home's chip row)
//   sensor.office_panel_printers x1c_page_all  "v␟head␟job␟temps␟foot␟alert␟row1␟row2␟row3…"
// ␟ is \x1f. The leading "v" stops HA trimming empty edge fields. Change the format there and this
// file breaks: office-panel/README.md lists this panel as a second consumer.
//
// Elapsed/remaining times arrive as live tokens "§<kind><epoch>§" that the panel counts itself
// (text()/tick()), so HA only sends when something changes. A trimmed port of office-panel/panel.h.
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "lvgl.h"

namespace feed {

// home35's palette (themes/main.yaml) plus the status colours the layout already uses
static const uint32_t PAGE = 0x42495A, CARD = 0x5A6173, TEXT = 0xFFFFFF, SUB = 0xC9CED8, DIM = 0x9AA0AE,
                      INFO_BG = 0x5A6173, WARN_BG = 0x5C4410, WARN_TEXT = 0xFFD27A, ACT_BG = 0xD32F2F,
                      AMBER = 0xFFAF00, GREEN = 0x4CAF50, GREEN_BG = 0x2E5E3A, BLUE_BG = 0x2D4A7A,
                      RED_TEXT = 0xFF8A80, OFF_BG = 0x4A5060;

inline std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t p = s.find(d, start);
    out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
    if (p == std::string::npos) break;
    start = p + 1;
  }
  return out;
}
inline std::string part(const std::vector<std::string> &v, size_t i) { return i < v.size() ? v[i] : ""; }
// a bundled attribute: drop the leading "v" field
inline std::vector<std::string> fields(const std::string &s) {
  auto v = split(s, '\x1f');
  if (!v.empty()) v.erase(v.begin());
  return v;
}

// feed icon names -> MDI glyphs; every one is in layouts/fonts/glyphs-home.yaml
inline const char *glyph(const std::string &n) {
  static const std::map<std::string, const char *> m = {
      {"night", "\U000F0594"},     {"sunny", "\U000F0599"},    {"partly", "\U000F0595"},
      {"cloudy", "\U000F0590"},    {"rainy", "\U000F0597"},    {"pouring", "\U000F0596"},
      {"fog", "\U000F0591"},       {"windy", "\U000F059D"},    {"lightning", "\U000F0593"},
      {"snowy", "\U000F0598"},     {"hail", "\U000F0592"},     {"alert", "\U000F0026"},
      {"wxalert", "\U000F0F2F"},   {"shield", "\U000F0499"},   {"shieldalert", "\U000F0ECC"},
      {"homealert", "\U000F15D0"}, {"door", "\U000F081C"},     {"window", "\U000F11DC"},
      {"flood", "\U000F1502"},     {"pump", "\U000F058F"},     {"smoke", "\U000F1930"},
      {"camoff", "\U000F185F"},    {"battery", "\U000F0083"},  {"garage", "\U000F12D4"},
      {"lights", "\U000F1253"},    {"trash", "\U000F0A7A"},    {"dryer", "\U000F0917"},
      {"car", "\U000F010B"},       {"package", "\U000F03D7"},  {"humidity", "\U000F058E"},
  };
  auto it = m.find(n);
  return it == m.end() ? "\U000F0026" : it->second;
}

inline void show(lv_obj_t *o, bool v) {
  if (v == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (v)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
// setters skip no-op updates: every feed refresh re-sends everything, and a needless set still
// invalidates and redraws the widget
inline void set_raw(lv_obj_t *o, const std::string &s) {
  const char *cur = lv_label_get_text(o);
  if (cur != nullptr && s == cur) return;
  lv_label_set_text(o, s.c_str());
}
inline void fg(lv_obj_t *o, uint32_t c) {
  if (lv_color_to_u32(lv_obj_get_style_text_color(o, LV_PART_MAIN)) == (0xFF000000 | c)) return;
  lv_obj_set_style_text_color(o, lv_color_hex(c), LV_PART_MAIN);
}
inline void bg(lv_obj_t *o, uint32_t c) {
  if (lv_color_to_u32(lv_obj_get_style_bg_color(o, LV_PART_MAIN)) == (0xFF000000 | c)) return;
  lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
}

// ── live time tokens ──────────────────────────────────────────────────────────────────────────
// kinds: d since "3 h 05 m"/"32 min", m whole minutes since, a since in words, u "in 3 h 05 m" or
// "soon", r remaining "3 h 05 m"
static const char *const TOKEN = "\xC2\xA7";  // §
inline int64_t &now_ref() {
  static int64_t t = 0;
  return t;
}
inline bool &frozen() {  // a preview scenario pins the clock (tools/previews/feed-*.yaml)
  static bool b = false;
  return b;
}
inline bool set_now(int64_t t) {  // true when the minute changed
  if (frozen()) return false;
  bool tick = t / 60 != now_ref() / 60;
  now_ref() = t;
  return tick;
}
inline std::string dur(int64_t s) {
  int64_t m = (s + 30) / 60;
  if (m < 0) m = 0;
  char b[24];
  if (m >= 60)
    snprintf(b, sizeof b, "%lld h %02lld m", (long long) (m / 60), (long long) (m % 60));
  else
    snprintf(b, sizeof b, "%lld min", (long long) m);
  return b;
}
inline std::string words(int64_t s) {
  if (s < 0) s = 0;
  static const struct { int64_t secs; const char *unit; } units[] = {
      {31536000, "year"}, {2592000, "month"}, {86400, "day"}, {3600, "hour"}, {60, "minute"}, {1, "second"}};
  for (auto &u : units)
    if (s >= u.secs || u.secs == 1) {
      int64_t n = s / u.secs;
      char b[32];
      snprintf(b, sizeof b, "%lld %s%s", (long long) n, u.unit, n == 1 ? "" : "s");
      return b;
    }
  return "";
}
inline std::string expand(const std::string &s) {
  int64_t now = now_ref();
  std::string out;
  size_t pos = 0;
  while (true) {
    size_t a = s.find(TOKEN, pos);
    size_t b = a == std::string::npos ? a : s.find(TOKEN, a + 2);
    if (b == std::string::npos) return out + s.substr(pos);
    out += s.substr(pos, a - pos);
    char kind = a + 2 < s.size() ? s[a + 2] : ' ';
    int64_t ts = strtoll(s.c_str() + a + 3, nullptr, 10);
    if (now == 0 || ts == 0) {
      out += "—";
    } else {
      int64_t since = now - ts, until = ts - now;
      switch (kind) {
        case 'd': out += dur(since); break;
        case 'm': out += std::to_string((long long) (since < 0 ? 0 : since / 60)); break;
        case 'a': out += words(since); break;
        case 'u': out += until > 0 ? "in " + dur(until) : std::string("soon"); break;
        case 'r': out += dur(until > 0 ? until : 0); break;
        default: break;
      }
    }
    pos = b + 2;
  }
}
inline std::vector<std::pair<lv_obj_t *, std::string>> &live() {
  static auto *v = new std::vector<std::pair<lv_obj_t *, std::string>>();
  return *v;
}
inline void text(lv_obj_t *o, const std::string &s) {
  bool tok = s.find(TOKEN) != std::string::npos;
  auto &l = live();
  auto it = std::find_if(l.begin(), l.end(), [o](const std::pair<lv_obj_t *, std::string> &e) { return e.first == o; });
  if (tok) {
    if (it == l.end())
      l.emplace_back(o, s);
    else
      it->second = s;
  } else if (it != l.end()) {
    l.erase(it);
  }
  set_raw(o, tok ? expand(s) : s);
}
inline void tick() {
  for (auto &e : live()) set_raw(e.first, expand(e.second));
}

// ── chips ─────────────────────────────────────────────────────────────────────────────────────
// The same chips, wording and colours as My Home, but ordered red, then amber, then the rest:
// on a 320px strip the tail scrolls off, and an amber chip off-screen is no warning at all.
struct Chip {
  std::string icon, level, text;
};
inline std::vector<Chip> parse_chips(const std::string &all) {
  std::vector<Chip> out;
  for (auto &f : fields(all)) {
    auto p = split(f, '|');
    if (p.size() >= 3 && !p[2].empty()) out.push_back({p[0], p[1], p[2]});
  }
  auto rank = [](const Chip &c) { return c.level == "act" ? 0 : c.level == "warn" ? 1 : 2; };
  std::stable_sort(out.begin(), out.end(), [&](const Chip &a, const Chip &b) { return rank(a) < rank(b); });
  return out;
}
// The strip never scrolls (2026-10-14): hidden chips were unreadable. Routine (info) chips shrink to an
// icon plus a short value; amber and red keep their full words. Whatever still doesn't fit collapses
// into a "+N" chip, and tapping the strip opens the full list (pages/chips.yaml).
inline std::string short_text(const Chip &c) {
  const std::string &t = c.text;
  if (c.level != "info") return t;
  auto last_word = [&t]() {
    size_t sp = t.rfind(' ');
    std::string w = sp == std::string::npos ? t : t.substr(sp + 1);
    if (!w.empty() && w[0] >= 'a' && w[0] <= 'z') w[0] -= 32;
    return w;
  };
  if (c.icon == "shield") return t == "Alarm off" ? std::string() : last_word();  // "Armed night" -> "Night"
  if (c.icon == "trash") return last_word();                                       // "Trash tomorrow" -> "Tomorrow"
  if (!t.empty() && t[0] >= '0' && t[0] <= '9') return t.substr(0, t.find(' '));    // "62° Clear" -> "62°", "2 lights" -> "2"
  return "";                                                                        // the icon says it
}
// boxes[i] holds icons[i] and texts[i]; unused slots hide
inline void set_chips(const std::vector<Chip> &chips, lv_obj_t **boxes, lv_obj_t **icons, lv_obj_t **texts, int n) {
  for (int i = 0; i < n; i++) {
    if (i >= (int) chips.size()) {
      show(boxes[i], false);
      continue;
    }
    const Chip &c = chips[i];
    bg(boxes[i], c.level == "act" ? ACT_BG : c.level == "warn" ? WARN_BG : INFO_BG);
    uint32_t ink = c.level == "act" ? TEXT : c.level == "warn" ? WARN_TEXT : SUB;
    fg(icons[i], ink);
    fg(texts[i], ink);
    text(icons[i], glyph(c.icon));
    std::string t = short_text(c);
    text(texts[i], t);
    show(texts[i], !t.empty());
    show(boxes[i], true);
  }
}
// Hide trailing chips that don't fit, and say how many with the "+N" chip, coloured by the most serious
// chip it hides (an amber "+2" still says "something needs a look").
inline void fit_chips(lv_obj_t *strip, lv_obj_t **boxes, int count, lv_obj_t *more, lv_obj_t *more_tx,
                      const std::vector<Chip> &chips) {
  const int GAP = 6;  // pad_column in chip_strip.yaml
  show(more, false);
  lv_obj_update_layout(strip);
  int avail = lv_obj_get_content_width(strip), total = 0;
  std::vector<int> w(count);
  for (int i = 0; i < count; i++) total += (w[i] = lv_obj_get_width(boxes[i])) + (i ? GAP : 0);
  if (total <= avail) return;
  text(more_tx, "+" + std::to_string(count));  // widest case, to measure
  show(more, true);
  lv_obj_update_layout(strip);
  int wm = lv_obj_get_width(more), used = 0, k = 0;
  while (k < count && used + w[k] + GAP + wm <= avail) used += w[k++] + GAP;
  int worst = 2;  // 0 act, 1 warn, 2 info
  for (int i = k; i < count; i++) {
    show(boxes[i], false);
    const std::string &l = chips[i].level;
    worst = std::min(worst, l == "act" ? 0 : l == "warn" ? 1 : 2);
  }
  bg(more, worst == 0 ? ACT_BG : worst == 1 ? WARN_BG : INFO_BG);
  fg(more_tx, worst == 1 ? WARN_TEXT : TEXT);
  text(more_tx, "+" + std::to_string(count - k));
}
// the chip list page: every chip in full words, same colours
inline void set_list(const std::vector<Chip> &chips, lv_obj_t **boxes, lv_obj_t **icons, lv_obj_t **texts, int n) {
  for (int i = 0; i < n; i++) {
    if (i >= (int) chips.size()) {
      show(boxes[i], false);
      continue;
    }
    const Chip &c = chips[i];
    bg(boxes[i], c.level == "act" ? ACT_BG : c.level == "warn" ? WARN_BG : INFO_BG);
    uint32_t ink = c.level == "act" ? TEXT : c.level == "warn" ? WARN_TEXT : TEXT;
    fg(icons[i], ink);
    fg(texts[i], ink);
    text(icons[i], glyph(c.icon));
    text(texts[i], c.text);
    show(boxes[i], true);
  }
}
inline bool &fit_dirty() {  // chips changed: refit on the strip's next tick while it's visible
  static bool b = false;
  return b;
}
inline bool &list_showing() {  // set by the chip list page's on_load / on_unload
  static bool b = false;
  return b;
}

// ── urgent takeover ───────────────────────────────────────────────────────────────────────────
// A red ("act") chip takes the screen: smoke, flood, the alarm going off or tampered, the garage
// open after dark or for 30 min, a red weather alert. Each alert takes over once; OK acknowledges
// the set on screen, and only an alert not in that set takes over again. The raw text is the key
// (tokens unexpanded), so "Garage open 31 min" -> "32 min" is the same alert.
inline std::vector<Chip> &chips() {  // the latest chips_all, parsed
  static auto *v = new std::vector<Chip>();
  return *v;
}
inline bool &alert_showing() {  // set by the alert page's on_load / on_unload
  static bool b = false;
  return b;
}
inline bool life_safety(const Chip &c) { return c.icon == "smoke" || c.icon == "flood" || c.icon == "shieldalert"; }
inline std::vector<Chip> act_chips(const std::vector<Chip> &chips) {
  std::vector<Chip> out;
  for (auto &c : chips)
    if (c.level == "act") out.push_back(c);
  return out;
}
inline std::string &acked() {  // "\n"-joined raw texts acknowledged with OK
  static auto *s = new std::string();
  return *s;
}
inline bool has_unacked(const std::vector<Chip> &act) {
  for (auto &c : act)
    if (acked().find(c.text) == std::string::npos) return true;
  return false;
}
inline void ack(const std::vector<Chip> &act) {
  acked().clear();
  for (auto &c : act) acked() += c.text + "\n";
}

// ── printers ──────────────────────────────────────────────────────────────────────────────────
inline uint32_t hex(const std::string &s, uint32_t fallback) {
  if (s.size() < 7 || s[0] != '#') return fallback;
  return strtoul(s.c_str() + 1, nullptr, 16) & 0xFFFFFF;
}
// relative luminance picks black or white ink (the rule tray_color.h uses)
inline uint32_t ink(uint32_t rgb) {
  float r = ((rgb >> 16) & 0xFF) / 255.0f, g = ((rgb >> 8) & 0xFF) / 255.0f, b = (rgb & 0xFF) / 255.0f;
  return 0.2126f * r + 0.7152f * g + 0.0722f * b > 0.5f ? 0x111111 : 0xFFFFFF;
}

struct Card {
  lv_obj_t *pill, *pill_lbl, *task, *bar, *time, *alert;
};
// head "state|status|task", job "pct|left|done|line", foot, alert
inline void set_card(const Card &w, const std::vector<std::string> &f) {
  auto head = split(part(f, 0), '|');
  auto job = split(part(f, 1), '|');
  std::string state = part(head, 0), status = part(head, 1), task = part(head, 2);
  int pct = atoi(part(job, 0).c_str());
  bool running = state == "active" || state == "paused";
  uint32_t pb = OFF_BG, pt = SUB;
  if (state == "active" || state == "paused") pb = WARN_BG, pt = WARN_TEXT;
  else if (state == "failed") pb = ACT_BG, pt = TEXT;
  else if (state == "done") pb = GREEN_BG, pt = TEXT;
  else if (state == "starting") pb = BLUE_BG, pt = TEXT;
  else if (state == "idle") pb = OFF_BG, pt = TEXT;
  bg(w.pill, pb);
  fg(w.pill_lbl, pt);
  text(w.pill_lbl, running ? status + " " + std::to_string(pct) + "%" : status);
  text(w.task, task.empty() ? std::string(state == "off" ? "" : "No job") : task);
  show(w.task, !task.empty() || state != "off");
  show(w.bar, pct >= 0 && (running || state == "done" || state == "failed"));
  if (pct >= 0) lv_bar_set_value(w.bar, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(w.bar, lv_color_hex(state == "failed" ? ACT_BG : state == "done" ? GREEN : AMBER),
                            LV_PART_INDICATOR);
  // one line: time left and finish while printing, the finish after, else the auto-off note
  std::string t;
  if (!part(job, 1).empty())
    t = part(job, 1) + (part(job, 2).empty() ? "" : " · " + part(job, 2));
  else if (!part(job, 2).empty())
    t = part(job, 2);
  else
    t = part(f, 3);
  if (state == "off" && t.rfind("Auto-off when on", 0) == 0) t = "";  // says nothing while off
  text(w.time, t);
  show(w.time, !t.empty());
  text(w.alert, part(f, 4));
  show(w.alert, !part(f, 4).empty());
}

struct Row {
  lv_obj_t *box, *label, *meta;
  lv_obj_t *sw[4], *amt[4];
};
// row "label|meta|level;#rrggbb|type|amount|flags;…" -- flags a = feeding, e = empty
inline void set_row(const Row &w, const std::string &v) {
  if (v.empty()) {
    show(w.box, false);
    return;
  }
  auto tiles = split(v, ';');
  auto h = split(tiles[0], '|');
  std::string l = part(h, 0);
  if (l.rfind("AMS ", 0) == 0) l = l.substr(4);
  else if (l == "Spool") l = "Ext";
  text(w.label, l);
  std::string meta = part(h, 1), level = part(h, 2);
  bool drying = meta.rfind("Drying", 0) == 0;
  text(w.meta, drying ? std::string("Drying") : split(meta, ' ')[0]);  // "16% · 23°" -> "16%"
  fg(w.meta, level == "warn" ? AMBER : SUB);
  for (int i = 0; i < 4; i++) {
    std::string t = i + 1 < (int) tiles.size() ? tiles[i + 1] : "";
    show(w.sw[i], !t.empty());
    if (t.empty()) continue;
    auto p = split(t, '|');
    bool empty = part(p, 3).find('e') != std::string::npos, feeding = part(p, 3).find('a') != std::string::npos;
    uint32_t c = hex(part(p, 0), 0x000000);
    std::string amt = part(p, 2);
    if (amt.size() > 2 && amt.compare(amt.size() - 2, 2, " g") == 0) amt.resize(amt.size() - 2);
    if (empty) {
      lv_obj_set_style_bg_opa(w.sw[i], LV_OPA_TRANSP, LV_PART_MAIN);
      fg(w.amt[i], DIM);
      text(w.amt[i], "—");
    } else {
      lv_obj_set_style_bg_opa(w.sw[i], LV_OPA_COVER, LV_PART_MAIN);
      bg(w.sw[i], c);
      fg(w.amt[i], ink(c));
      text(w.amt[i], amt);  // blank for a spool with no RFID and no Bambuddy weight
    }
    lv_obj_set_style_border_color(w.sw[i], lv_color_hex(feeding ? AMBER : 0x2A2F3A), LV_PART_MAIN);
    lv_obj_set_style_border_width(w.sw[i], feeding ? 3 : 1, LV_PART_MAIN);
  }
  show(w.box, true);
}

}  // namespace feed
