#include "config.h"
#if WITH_NAGGER
#include "NaggerMode.h"
#include "NaggerClient.h"
#include "Gfx.h"
#include <Arduino_GFX_Library.h>
#include <math.h>

NaggerMode g_naggerMode;

// U8g2 fonts with Latin Extended-A (Polish letters), from the U8g2 library.
extern "C" {
extern const uint8_t u8g2_font_helvB08_te[];
extern const uint8_t u8g2_font_helvB10_te[];
extern const uint8_t u8g2_font_helvR12_te[];
extern const uint8_t u8g2_font_helvB12_te[];
extern const uint8_t u8g2_font_helvB14_te[];
extern const uint8_t u8g2_font_helvB18_te[];
extern const uint8_t u8g2_font_logisoso42_tn[];
}

// Nagger's wall colours (RGB565), through the Display tab's colour correction.
#define K_BLACK      gfxTint(0x0000)
#define K_RED        gfxTint(0xF943)   // #FF2A1A
#define K_DIM        gfxTint(0x2840)   // #2A0805
#define K_AMBER      gfxTint(0xFD80)   // #FFB000
#define K_ORANGE     gfxTint(0xFBC3)   // #FF7A1A
#define K_TEXT       gfxTint(0xEF5D)   // #E8E8E8
#define K_WHITE      gfxTint(0xFFFF)
#define K_MUTED      gfxTint(0x8C51)   // #8A8A8A
#define K_LINE       gfxTint(0x18E3)   // #1A1A1A
#define K_GREY       gfxTint(0x52AA)   // #555555
#define K_BADGE      gfxTint(0x2104)   // #222222
#define K_VIOLET     gfxTint(0xC3FF)   // #C77DFF
#define K_VMETA      gfxTint(0x8BD4)   // #8E7AA0
#define K_SLACKROW   gfxTint(0x1063)   // #140E1A
#define K_SLACKESC   gfxTint(0x28A8)   // #2A1640

static const char* const kWeekdays[] = {"nd", "pon", "wt", "śr", "czw", "pt", "sob"};

// The studio clock fills the screen: second dots, hour dots, amber arc (Nagger's ClockFace
// proportions: radii 0.43 / 0.38 / 0.475 of the size).
static const int16_t RC = 120;
static int16_t g_secX[60], g_secY[60], g_arcX[60], g_arcY[60], g_hourX[12], g_hourY[12];

static Arduino_GFX* G() { return gfxDev(); }

// ---- text -----------------------------------------------------------------
static void font(const uint8_t* f) {
  G()->setFont(f);
  G()->setUTF8Print(true);
  G()->setTextSize(1);
}

static int16_t textW(const char* s) {
  int16_t x1, y1;
  uint16_t w, h;
  G()->getTextBounds(s, 0, 100, &x1, &y1, &w, &h);
  return (int16_t)w;
}

static void text(int x, int baseline, const char* s, uint16_t color) {
  G()->setTextColor(color);
  G()->setCursor(x, baseline);
  G()->print(s);
}

static void textCentered(int cx, int baseline, const char* s, uint16_t color) {
  text(cx - textW(s) / 2, baseline, s, color);
}

static void textRight(int right, int baseline, const char* s, uint16_t color) {
  text(right - textW(s), baseline, s, color);
}

// Drops the last UTF-8 character of `s` in place.
static void dropLastChar(char* s) {
  size_t n = strlen(s);
  while (n > 0) {
    n--;
    if (((uint8_t)s[n] & 0xC0) != 0x80) break;   // reached a lead byte
  }
  s[n] = 0;
}

static void ellipsize(char* line, size_t size, int w) {
  char probe[NAGGER_TEXT + 8];
  for (;;) {
    snprintf(probe, sizeof(probe), "%s...", line);
    if (textW(probe) <= w || !line[0]) break;
    dropLastChar(line);
  }
  // trailing spaces before the dots look odd
  size_t n = strlen(line);
  while (n && line[n - 1] == ' ') line[--n] = 0;
  strlcat(line, "...", size);
}

// Up to maxLines lines of `s` within width w, first baseline y, then every `step`
// pixels; the last line ends in "..." when the text does not fit. Returns lines drawn.
static int wrap(const char* s, int x, int y, int w, int step, int maxLines, uint16_t color) {
  char line[NAGGER_TEXT + 8];
  const char* p = s;
  int lines = 0;
  while (*p && lines < maxLines) {
    while (*p == ' ') p++;
    if (!*p) break;
    size_t fit = 0;                         // bytes of p that fit on this line
    const char* q = p;
    while (*q) {
      const char* end = q;
      while (*end && *end != ' ') end++;
      size_t n = end - p;
      if (n >= sizeof(line) - 4) break;
      memcpy(line, p, n);
      line[n] = 0;
      if (textW(line) > w) break;
      fit = n;
      q = end;
      while (*q == ' ') q++;
    }
    if (fit == 0) {                          // one word wider than the line: cut it
      size_t n = 0;
      while (p[n] && n < sizeof(line) - 4) {
        size_t c = 1;
        while ((((uint8_t)p[n + c]) & 0xC0) == 0x80) c++;
        memcpy(line, p, n + c);
        line[n + c] = 0;
        if (textW(line) > w) break;
        n += c;
      }
      fit = n ? n : 1;
    }
    memcpy(line, p, fit);
    line[fit] = 0;
    const char* rest = p + fit;
    while (*rest == ' ') rest++;
    if (lines == maxLines - 1 && *rest) ellipsize(line, sizeof(line), w);
    text(x, y + lines * step, line, color);
    lines++;
    p = rest;
  }
  return lines;
}

// ---- time -----------------------------------------------------------------
static void countdown(char* out, size_t size, uint32_t secs) {
  uint32_t h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
  if (h) snprintf(out, size, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else   snprintf(out, size, "%02u:%02u", (unsigned)m, (unsigned)s);
}

static void hhmm(char* out, size_t size, uint32_t epoch) {
  struct tm t;
  naggerLocal(epoch, t);
  snprintf(out, size, "%02d:%02d", t.tm_hour, t.tm_min);
}

// "dziś 13:00", "jutro 10:00", "pon 12:00" (Nagger's Format.dayTime).
static void dayTime(char* out, size_t size, uint32_t epoch, uint32_t now) {
  struct tm a, b;
  naggerLocal(now, a);
  naggerLocal(epoch, b);
  char hm[8];
  snprintf(hm, sizeof(hm), "%02d:%02d", b.tm_hour, b.tm_min);
  int days = (b.tm_year - a.tm_year) * 366 + (b.tm_yday - a.tm_yday);
  if (days == 0)      snprintf(out, size, "dziś %s", hm);
  else if (days == 1) snprintf(out, size, "jutro %s", hm);
  else                snprintf(out, size, "%s %s", kWeekdays[b.tm_wday % 7], hm);
}

// What the clock screen says about the meeting, and the amber arc in minutes.
struct Lead { char text[24]; uint16_t color; int arc; };

static Lead lead(const NaggerData& d, uint32_t now) {
  Lead l = {"", K_AMBER, 0};
  if (!d.valid || !now) return l;
  if (!d.c.hasMeeting) {
    strlcpy(l.text, "Brak spotkań", sizeof(l.text));
    l.color = K_MUTED;
    return l;
  }
  const NaggerMeeting& m = d.c.m;
  if (m.s <= now && now < m.e) {
    char cd[12];
    countdown(cd, sizeof(cd), m.e - now);
    snprintf(l.text, sizeof(l.text), "ON AIR %s", cd);
    l.color = K_RED;
    l.arc = (int)min<uint32_t>(60, (m.e - now + 59) / 60);
  } else if (now < m.s) {
    uint32_t d2 = m.s - now;
    if (d2 < 3600) {
      char cd[12];
      countdown(cd, sizeof(cd), d2);
      snprintf(l.text, sizeof(l.text), "za %s", cd);
      l.arc = (int)((d2 + 59) / 60);
    } else {
      dayTime(l.text, sizeof(l.text), m.s, now);
    }
  }
  return l;
}

static bool stale(const NaggerData& d) { return !d.valid || naggerAgeSec() > 180; }

// ---- pieces ---------------------------------------------------------------
static uint16_t priorityColor(uint8_t c) {
  switch (c) {
    case 1:  return K_RED;
    case 2:  return K_ORANGE;
    case 3:  return K_AMBER;
    default: return K_GREY;
  }
}

static void header(const char* left, uint16_t color) {
  font(u8g2_font_helvB10_te);
  text(10, 20, left, color);
}

static void drawSecondDot(int i, bool on) {
  G()->fillCircle(g_secX[i], g_secY[i], 3, on ? K_RED : K_DIM);
}

static void drawArc(int arc) {
  for (int i = 0; i < 60; i++) G()->fillCircle(g_arcX[i], g_arcY[i], 2, i < arc ? K_AMBER : K_BLACK);
}

static void drawStudioDigits(uint32_t now) {
  G()->fillRect(RC - 72, RC - 28, 144, 48, K_BLACK);
  if (!now) return;
  char hm[8];
  hhmm(hm, sizeof(hm), now);
  font(u8g2_font_logisoso42_tn);
  textCentered(RC, RC + 18, hm, K_RED);
}

static void drawStudioSeconds(uint32_t now) {
  G()->fillRect(RC - 20, RC + 28, 40, 22, K_BLACK);
  if (!now) return;
  struct tm t;
  naggerLocal(now, t);
  char ss[4];
  snprintf(ss, sizeof(ss), "%02d", t.tm_sec);
  font(u8g2_font_helvB14_te);
  textCentered(RC, RC + 46, ss, K_RED);
}

// Screen 2: the time in big digits, the next meeting, then as many tasks as fit.
static void drawTimeDigits(uint32_t now) {
  G()->fillRect(0, 4, 240, 52, K_BLACK);
  if (!now) return;
  char hm[8];
  hhmm(hm, sizeof(hm), now);
  font(u8g2_font_logisoso42_tn);
  textCentered(120, 52, hm, K_RED);
}

// The lead line as last drawn, so the countdown repaints only when its text changes.
static char     g_leadText[24];
static uint16_t g_leadColor;

static void drawTimeLead(const Lead& l, bool force) {
  if (!force && l.color == g_leadColor && !strcmp(l.text, g_leadText)) return;
  strlcpy(g_leadText, l.text, sizeof(g_leadText));
  g_leadColor = l.color;
  G()->fillRect(0, 64, 240, 26, K_BLACK);
  font(u8g2_font_helvB14_te);
  text(10, 84, l.text, l.color);
}

static void drawTimeTasks(const NaggerData& d) {
  G()->fillRect(0, 138, 240, 102, K_BLACK);
  if (stale(d)) {
    char msg[40];
    if (d.valid) snprintf(msg, sizeof(msg), "Brak Naggera od %u min", (unsigned)(naggerAgeSec() / 60));
    else         strlcpy(msg, "Czekam na Naggera", sizeof(msg));
    font(u8g2_font_helvB12_te);
    textCentered(120, 190, msg, d.valid ? K_RED : K_MUTED);
    return;
  }
  // In progress first, then to do: one line each.
  const NaggerTask* rows[4];
  int n = 0;
  for (int i = 0; i < d.c.nProg && n < 4; i++) rows[n++] = &d.c.prog[i];
  for (int i = 0; i < d.c.nTodo && n < 4; i++) rows[n++] = &d.c.todo[i];
  font(u8g2_font_helvR12_te);
  for (int i = 0; i < n; i++) {
    int y = 142 + i * 24;
    G()->fillRect(10, y + 2, 4, 18, priorityColor(rows[i]->c));
    wrap(rows[i]->t, 22, y + 16, 210, 17, 1, rows[i]->k == 1 ? K_WHITE : K_TEXT);
  }
}

// ---- screens ----------------------------------------------------------------
// Screen 1: only the studio clock, Nagger's ClockFace without the label.
static void drawStudio(const NaggerData& d, uint32_t now) {
  struct tm t;
  if (now) naggerLocal(now, t);
  int sec = now ? t.tm_sec : -1;
  for (int i = 0; i < 60; i++) drawSecondDot(i, i <= sec);
  for (int i = 0; i < 12; i++) G()->fillCircle(g_hourX[i], g_hourY[i], 3, K_RED);
  drawArc(lead(d, now).arc);
  drawStudioDigits(now);
  drawStudioSeconds(now);
}

static void drawTime(const NaggerData& d, uint32_t now) {
  drawTimeDigits(now);
  drawTimeLead(lead(d, now), true);
  font(u8g2_font_helvB12_te);
  if (d.valid && d.c.hasMeeting) wrap(d.c.m.t, 10, 108, 220, 18, 2, K_WHITE);
  G()->drawFastHLine(0, 136, 240, K_LINE);
  drawTimeTasks(d);
}

// In progress / to do: four tasks in 14 px bold, two lines each (12 px regular
// was too small to read, 18 px too big).
static void drawTasks(const char* title, const NaggerTask* tasks, uint8_t count, uint16_t total, uint16_t overdue, bool badges) {
  char h[40];
  snprintf(h, sizeof(h), "%s · %u", title, total);
  font(u8g2_font_helvB12_te);
  text(8, 21, h, K_MUTED);
  if (overdue) {
    char o[24];
    snprintf(o, sizeof(o), "po terminie %u", overdue);
    font(u8g2_font_helvB10_te);
    textRight(232, 20, o, K_RED);
  }
  if (count > 4) count = 4;
  for (int i = 0; i < count; i++) {
    const NaggerTask& t = tasks[i];
    int y = 32 + i * 51;
    G()->fillRect(8, y + 2, 4, 42, priorityColor(t.c));
    int width = 218;
    if (badges && t.b[0] && t.k) {
      font(u8g2_font_helvB10_te);
      int bw = textW(t.b) + 8;
      uint16_t bg = t.k == 1 ? K_RED : t.k == 2 ? K_AMBER : K_BADGE;
      uint16_t fg = t.k == 3 ? K_TEXT : K_BLACK;
      G()->fillRoundRect(234 - bw, y + 2, bw, 16, 3, bg);
      text(238 - bw, y + 14, t.b, fg);
      width -= bw + 4;
    }
    bool emph = t.k == 1 || t.k == 2 || t.c == 1;
    font(u8g2_font_helvB14_te);
    wrap(t.t, 18, y + 16, width, 20, 2, emph ? K_WHITE : K_TEXT);
  }
}

static void drawSlack(const NaggerData& d) {
  char h[24];
  snprintf(h, sizeof(h), "SLACK · %u", d.c.totalSlack);
  header(h, K_VIOLET);
  for (int i = 0; i < d.c.nSlack; i++) {
    const NaggerSlack& s = d.c.slack[i];
    int y = 30 + i * 69;
    G()->fillRoundRect(6, y, 228, 64, 4, s.x ? K_SLACKESC : K_SLACKROW);
    G()->fillRect(10, y + 6, 4, 52, s.x ? K_VIOLET : K_VMETA);
    font(s.x ? u8g2_font_helvB12_te : u8g2_font_helvR12_te);
    wrap(s.t, 20, y + 19, 208, 17, 2, s.x ? K_WHITE : K_TEXT);
    char meta[80];
    snprintf(meta, sizeof(meta), "%s · %s · %s", s.l, s.a, s.g);
    font(u8g2_font_helvB08_te);
    wrap(meta, 20, y + 56, 208, 10, 1, s.x ? K_VIOLET : K_VMETA);
  }
}

// ---- the override: Nagger's alert, a blinking Guru Meditation frame ------------
// The alert to show: Nagger's, or (when Nagger has gone quiet) a meeting closer
// than urgentMin from the last data, so the device still warns on its own.
static NaggerAlert currentAlert(const NaggerData& d, uint32_t now, uint8_t urgentMin) {
  NaggerAlert a = {};
  if (!d.valid) return a;
  if (!stale(d)) return d.c.alert;
  const NaggerMeeting& m = d.c.m;
  if (d.c.hasMeeting && now && urgentMin && now < m.s && m.s - now <= (uint32_t)urgentMin * 60) {
    a.k = NAGGER_ALERT_MEETING;
    strlcpy(a.t, m.t, sizeof(a.t));
    a.s = m.s;
    a.e = m.e;
  }
  return a;
}

static void drawFrame(bool on) {
  uint16_t c = on ? K_RED : K_BLACK;
  G()->fillRect(0, 0, 240, 6, c);
  G()->fillRect(0, 234, 240, 6, c);
  G()->fillRect(0, 0, 6, 240, c);
  G()->fillRect(234, 0, 6, 240, c);
}

static void drawAlertCountdown(const NaggerAlert& a, uint32_t now) {
  G()->fillRect(8, 60, 224, 60, K_BLACK);
  char cd[12];
  uint16_t color = K_AMBER;
  if (a.k == NAGGER_ALERT_ONAIR) {
    countdown(cd, sizeof(cd), a.e > now ? a.e - now : 0);
    color = K_RED;
  } else {
    uint32_t left = a.s > now ? a.s - now : 0;
    countdown(cd, sizeof(cd), left);
    if (left < 60) color = K_RED;
  }
  font(u8g2_font_logisoso42_tn);
  textCentered(120, 110, cd, color);
}

static void drawAlert(const NaggerAlert& a, uint32_t now) {
  switch (a.k) {
    case NAGGER_ALERT_MEETING:
      font(u8g2_font_helvB12_te);
      textCentered(120, 40, "SPOTKANIE ZA", K_MUTED);
      drawAlertCountdown(a, now);
      font(u8g2_font_helvB14_te);
      wrap(a.t, 16, 150, 208, 21, 3, K_WHITE);
      break;
    case NAGGER_ALERT_ONAIR:
      font(u8g2_font_helvB18_te);
      textCentered(120, 44, "ON AIR", K_RED);
      drawAlertCountdown(a, now);
      font(u8g2_font_helvB14_te);
      wrap(a.t, 16, 150, 208, 21, 3, K_WHITE);
      break;
    case NAGGER_ALERT_URGENT: {
      font(u8g2_font_helvB18_te);
      textCentered(120, 50, "PILNE", K_RED);
      font(u8g2_font_helvB14_te);
      wrap(a.t, 16, 92, 208, 22, 5, K_WHITE);
      if (a.n) {
        char more[24];
        snprintf(more, sizeof(more), "+%u więcej", a.n);
        font(u8g2_font_helvB10_te);
        textCentered(120, 222, more, K_MUTED);
      }
      break;
    }
    default:
      break;
  }
}

// ---- mode -----------------------------------------------------------------
void NaggerMode::begin(const Settings& s) {
  for (int i = 0; i < 60; i++) {
    float a = (i * 6 - 90) * (float)M_PI / 180.0f;
    g_secX[i] = RC + (int16_t)lroundf(103 * cosf(a));
    g_secY[i] = RC + (int16_t)lroundf(103 * sinf(a));
    g_arcX[i] = RC + (int16_t)lroundf(114 * cosf(a));
    g_arcY[i] = RC + (int16_t)lroundf(114 * sinf(a));
  }
  for (int i = 0; i < 12; i++) {
    float a = (i * 30 - 90) * (float)M_PI / 180.0f;
    g_hourX[i] = RC + (int16_t)lroundf(91 * cosf(a));
    g_hourY[i] = RC + (int16_t)lroundf(91 * sinf(a));
  }
  full_ = true;
  switchMs_ = millis();
}

bool NaggerMode::available(Screen sc) const {
  const NaggerData& d = naggerGet();
  switch (sc) {
    case STUDIO:   return true;
    case TIME:     return true;
    case PROGRESS: return d.valid && d.c.nProg;
    case TODO:     return d.valid && d.c.nTodo;
    case SLACK:    return d.valid && d.c.nSlack;
    default:       return false;
  }
}

NaggerMode::Screen NaggerMode::next(Screen from) const {
  const int count = SLACK + 1;   // the rotating screens, STUDIO..SLACK
  int base = (from >= count) ? -1 : (int)from;
  for (int hop = 1; hop <= count; hop++) {
    Screen cand = (Screen)((base + hop + count) % count);
    if (available(cand)) return cand;
  }
  return STUDIO;
}

void NaggerMode::drawFull(Screen sc, uint32_t now, const NaggerAlert& alert) {
  const NaggerData& d = naggerGet();
  G()->fillScreen(K_BLACK);
  switch (sc) {
    case STUDIO:   drawStudio(d, now); break;
    case TIME:     drawTime(d, now); break;
    case PROGRESS: drawTasks("W TOKU", d.c.prog, d.c.nProg, d.c.nProg, 0, false); break;
    case TODO:     drawTasks("DO ZROBIENIA", d.c.todo, d.c.nTodo, d.c.totalTodo, d.c.overdue, true); break;
    case SLACK:    drawSlack(d); break;
    case ALERT:    drawAlert(alert, now); frameOn_ = true; drawFrame(true); blinkMs_ = millis(); break;
    default:       break;
  }
  drawnVersion_ = d.version;
  lastSec_ = now;
  struct tm t;
  if (now) naggerLocal(now, t);
  lastMinute_ = now ? t.tm_min : -1;
  lastArc_ = lead(d, now).arc;
  lastStale_ = stale(d);
}

// Once a second: only what moved.
void NaggerMode::tick(Screen sc, uint32_t now, const NaggerAlert& alert) {
  const NaggerData& d = naggerGet();
  if (sc == ALERT) {
    if (alert.k != NAGGER_ALERT_URGENT) drawAlertCountdown(alert, now);
    return;
  }
  if (sc != STUDIO && sc != TIME) return;
  struct tm t;
  naggerLocal(now, t);
  bool minute = t.tm_min != lastMinute_;
  lastMinute_ = t.tm_min;
  Lead l = lead(d, now);
  if (sc == STUDIO) {
    if (t.tm_sec == 0) for (int i = 1; i < 60; i++) drawSecondDot(i, false);
    drawSecondDot(t.tm_sec, true);
    drawStudioSeconds(now);
    if (minute) drawStudioDigits(now);
    if (l.arc != lastArc_) {
      lastArc_ = l.arc;
      drawArc(l.arc);
    }
    return;
  }
  if (minute) drawTimeDigits(now);
  drawTimeLead(l, false);
  bool st = stale(d);
  if (st != lastStale_ || (st && t.tm_sec == 0)) {
    lastStale_ = st;
    drawTimeTasks(d);
  }
}

void NaggerMode::service(const Settings& s) {
  const NaggerData& d = naggerGet();
  uint32_t now = naggerNow();
  uint16_t rot = d.c.rot ? d.c.rot : s.nagger.rotateSec;
  if (rot < 3) rot = 3;

  NaggerAlert alert = currentAlert(d, now, s.nagger.urgentMin);
  bool alerting = alert.k != NAGGER_ALERT_NONE;
  Screen want = screen_;
  if (alerting) {
    want = ALERT;
  } else if (screen_ == NONE || screen_ == ALERT) {
    want = STUDIO;
    switchMs_ = millis();
  } else if (!available(screen_) || millis() - switchMs_ >= (uint32_t)rot * 1000UL) {
    want = next(screen_);
    switchMs_ = millis();
  }

  // A different alert (another meeting, another task) repaints too.
  bool alertChanged = alerting && (alert.k != shownAlertKind_ || alert.s != shownAlertStart_ ||
                                   strncmp(alert.t, shownAlertTitle_, sizeof(shownAlertTitle_)) != 0);
  if (want != screen_ || full_ || d.version != drawnVersion_ || alertChanged) {
    screen_ = want;
    full_ = false;
    shownAlertKind_ = alert.k;
    shownAlertStart_ = alert.s;
    strlcpy(shownAlertTitle_, alert.t, sizeof(shownAlertTitle_));
    drawFull(screen_, now, alert);
    return;
  }
  if (screen_ == ALERT && millis() - blinkMs_ >= 500) {
    blinkMs_ = millis();
    frameOn_ = !frameOn_;
    drawFrame(frameOn_);
  }
  if (now && now != lastSec_) {
    lastSec_ = now;
    tick(screen_, now, alert);
  }
}

#endif  // WITH_NAGGER
