// NaggerMode.h — Nagger's wall on the SmallTV (meetings, tasks, Slack).
//
// Nagger (a macOS app: meetings, Linear tasks, Slack) pushes its state to POST
// /api/nagger every few seconds; this mode rotates through the screens that have
// something on them: the studio clock alone, the time with the next meeting and
// tasks, today's meetings, in progress, to do, Slack. When Nagger is nagging on
// the Mac (a meeting about to start, ON AIR, a new urgent task) the rotation stops
// for a full screen with a blinking red frame; with Nagger silent for a while, a
// meeting within settings.nagger.urgentMin does the same from the last data.
// The clock ticks locally (SNTP, or Nagger's clock until it syncs).
#pragma once
#include "Mode.h"
#include "config.h"
#include "NaggerData.h"

class NaggerMode : public DisplayMode {
 public:
  enum Screen : uint8_t { STUDIO, TIME, MEETINGS, PROGRESS, TODO, SLACK, ALERT, NONE };

  const char* id() const override { return "nagger"; }
  uint8_t     modeConst() const override { return MODE_NAGGER; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override { full_ = true; }
  void wake(const Settings& s) override { full_ = true; switchMs_ = millis(); }

 private:
  bool    available(Screen sc) const;
  Screen  next(Screen from) const;
  void    drawFull(Screen sc, uint32_t now, const NaggerAlert& alert);
  void    tick(Screen sc, uint32_t now, const NaggerAlert& alert);

  Screen   screen_ = NONE;
  bool     full_ = true;
  uint32_t switchMs_ = 0;
  uint32_t drawnVersion_ = 0;
  uint32_t lastSec_ = 0;       // epoch second last drawn
  int      lastMinute_ = -1;
  int      lastArc_ = -1;
  bool     lastStale_ = false;
  // The override's blinking frame and which alert is on screen.
  bool     frameOn_ = false;
  uint32_t blinkMs_ = 0;
  uint8_t  shownAlertKind_ = 0;
  uint32_t shownAlertStart_ = 0;
  char     shownAlertTitle_[NAGGER_TEXT] = {0};
};

extern NaggerMode g_naggerMode;
