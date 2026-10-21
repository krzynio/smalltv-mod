// NaggerMode.h — Nagger's wall on the SmallTV (meetings, tasks, Slack).
//
// Nagger (github.com/krzynio/nagger) pushes its state to POST /api/nagger every
// few seconds; this mode rotates through the screens that have something on them:
// clock + next meeting, today's meetings, in progress, to do, Slack. A meeting
// starting within settings.nagger.urgentMin stops the rotation on a full-screen
// countdown. The clock ticks locally (SNTP, or Nagger's clock until it syncs).
#pragma once
#include "Mode.h"
#include "config.h"

class NaggerMode : public DisplayMode {
 public:
  enum Screen : uint8_t { CLOCK, MEETINGS, PROGRESS, TODO, SLACK, URGENT, NONE };

  const char* id() const override { return "nagger"; }
  uint8_t     modeConst() const override { return MODE_NAGGER; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override { full_ = true; }
  void wake(const Settings& s) override { full_ = true; switchMs_ = millis(); }

 private:
  bool    available(Screen sc) const;
  Screen  next(Screen from) const;
  void    drawFull(Screen sc, uint32_t now);
  void    tick(Screen sc, uint32_t now);

  Screen   screen_ = NONE;
  bool     full_ = true;
  uint32_t switchMs_ = 0;
  uint32_t drawnVersion_ = 0;
  uint32_t lastSec_ = 0;       // epoch second last drawn
  int      lastMinute_ = -1;
  int      lastArc_ = -1;
  bool     lastStale_ = false;
};

extern NaggerMode g_naggerMode;
