#pragma once
// Fork-only: the text of a tray pill when the panel shows spool weights.
//
// Bambuddy's remaining grams win whenever Bambuddy has a spool assigned to the
// slot (the HA sensor is unavailable otherwise, which arrives here as NaN). With
// no Bambuddy weight, the RFID `remain` percentage is shown as grams of a 1 kg
// spool, so a row never mixes units. `remain` is "-1" for a spool without RFID
// and "" before Home Assistant has sent anything; both read "--".
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace tray_weight {

inline std::string text(float grams, const std::string &rfid_remain) {
  if (!std::isnan(grams))
    return std::to_string(std::lround(std::max(grams, 0.0f)));
  const char *s = rfid_remain.c_str();
  char *end = nullptr;
  long pct = std::strtol(s, &end, 10);
  if (end == s || *end != '\0' || pct < 0)
    return "--";
  return std::to_string(pct * 10);
}

}  // namespace tray_weight
