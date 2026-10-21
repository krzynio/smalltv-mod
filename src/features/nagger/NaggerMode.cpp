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
extern const uint8_t u8g2_font_helvR08_te[];
extern const uint8_t u8g2_font_helvB08_te[];
extern const uint8_t u8g2_font_helvB10_te[];
extern const uint8_t u8g2_font_helvR12_te[];
extern const uint8_t u8g2_font_helvB12_te[];
extern const uint8_t u8g2_font_helvB14_te[];
extern const uint8_t u8g2_font_helvB18_tn[];
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

// Ring geometry of the clock screen (centre, radii), positions precomputed.
static const int16_t RC_X = 56, RC_Y = 56;
static int16_t g_secX[60], g_secY[60], g_arcX[60], g_arcY[60];

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
  if (!d.valid || !d.c.hasMeeting || !now) return l;
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
  G()->fillCircle(g_secX[i], g_secY[i], 1, on ? K_RED : K_DIM);
}

static void drawArc(int arc) {
  for (int i = 0; i < 60; i++) G()->fillCircle(g_arcX[i], g_arcY[i], 1, i < arc ? K_AMBER : K_BLACK);
}

static void drawClockDigits(uint32_t now) {
  G()->fillRect(RC_X - 32, RC_Y - 14, 64, 28, K_BLACK);
  if (!now) return;
  char hm[8];
  hhmm(hm, sizeof(hm), now);
  font(u8g2_font_helvB18_tn);
  textCentered(RC_X, RC_Y + 9, hm, K_RED);
}

static void drawLead(const Lead& l) {
  G()->fillRect(112, 8, 128, 26, K_BLACK);
  font(u8g2_font_helvB14_te);
  text(114, 28, l.text, l.color);
}

static void drawCounter(int cx, uint16_t value, const char* label, uint16_t color) {
  char n[8];
  snprintf(n, sizeof(n), "%u", value);
  font(u8g2_font_helvB18_tn);
  textCentered(cx, 210, n, color);
  font(u8g2_font_helvR08_te);
  textCentered(cx, 228, label, K_MUTED);
}

static void drawClockBottom(const NaggerData& d) {
  G()->fillRect(0, 116, 240, 124, K_BLACK);
  if (!d.valid) {
    font(u8g2_font_helvB12_te);
    textCentered(120, 160, "Czekam na Naggera", K_MUTED);
    return;
  }
  const NaggerContent& c = d.c;
  char h[32];
  snprintf(h, sizeof(h), "W TOKU · %u", c.nProg);
  font(u8g2_font_helvR08_te);
  text(8, 128, h, K_MUTED);
  if (c.nProg) {
    G()->fillRect(8, 136, 3, 34, priorityColor(c.prog[0].c));
    font(u8g2_font_helvR12_te);
    wrap(c.prog[0].t, 17, 150, 215, 17, 2, K_TEXT);
  } else {
    font(u8g2_font_helvR12_te);
    text(8, 152, "Nic w toku", K_MUTED);
  }
  if (stale(d)) {
    char msg[40];
    snprintf(msg, sizeof(msg), "Brak Naggera od %u min", (unsigned)(naggerAgeSec() / 60));
    font(u8g2_font_helvB10_te);
    textCentered(120, 214, msg, K_RED);
    return;
  }
  drawCounter(40, c.totalTodo, "DO ZROBIENIA", K_TEXT);
  drawCounter(120, c.overdue, "PO TERMINIE", c.overdue ? K_RED : K_MUTED);
  drawCounter(200, c.totalSlack, "SLACK", c.totalSlack ? K_VIOLET : K_MUTED);
}

// ---- screens ----------------------------------------------------------------
static void drawClock(const NaggerData& d, uint32_t now) {
  struct tm t;
  if (now) naggerLocal(now, t);
  int sec = now ? t.tm_sec : -1;
  for (int i = 0; i < 60; i++) drawSecondDot(i, i <= sec);
  for (int i = 0; i < 12; i++) {
    float a = (i * 30 - 90) * (float)M_PI / 180.0f;
    G()->fillCircle(RC_X + (int)lroundf(37 * cosf(a)), RC_Y + (int)lroundf(37 * sinf(a)), 2, K_RED);
  }
  Lead l = lead(d, now);
  drawArc(l.arc);
  drawClockDigits(now);
  drawLead(l);
  font(u8g2_font_helvB12_te);
  if (d.valid && d.c.hasMeeting) wrap(d.c.m.t, 114, 54, 122, 17, 3, K_WHITE);
  else if (d.valid) text(114, 54, "Brak spotkań", K_MUTED);
  G()->drawFastHLine(0, 114, 240, K_LINE);
  drawClockBottom(d);
}

static void drawMeetings(const NaggerData& d, uint32_t now) {
  header("SPOTKANIA DZIŚ", K_MUTED);
  const NaggerMeeting* list[1 + NAGGER_LATER];
  int n = 0;
  if (d.c.hasMeeting) list[n++] = &d.c.m;
  for (int i = 0; i < d.c.nLater; i++) list[n++] = &d.c.later[i];
  for (int i = 0; i < n; i++) {
    int y = 34 + i * 51;
    const NaggerMeeting& m = *list[i];
    bool onAir = m.s <= now && now < m.e;
    char hm[8];
    hhmm(hm, sizeof(hm), m.s);
    font(u8g2_font_helvB12_te);
    text(10, y + 14, onAir ? "teraz" : hm, onAir ? K_RED : K_AMBER);
    font(u8g2_font_helvR12_te);
    wrap(m.t, 62, y + 14, 170, 17, 2, onAir ? K_WHITE : K_TEXT);
  }
}

static void drawTasks(const char* title, const NaggerTask* tasks, uint8_t count, uint16_t total, uint16_t overdue, bool badges) {
  char h[40];
  snprintf(h, sizeof(h), "%s · %u", title, total);
  header(h, K_MUTED);
  if (overdue) {
    char o[24];
    snprintf(o, sizeof(o), "po terminie %u", overdue);
    font(u8g2_font_helvB08_te);
    textRight(230, 19, o, K_RED);
  }
  for (int i = 0; i < count; i++) {
    const NaggerTask& t = tasks[i];
    int y = 32 + i * 51;
    G()->fillRect(10, y + 2, 4, 40, priorityColor(t.c));
    int width = 212;
    if (badges && t.b[0] && t.k) {
      font(u8g2_font_helvB08_te);
      int bw = textW(t.b) + 8;
      uint16_t bg = t.k == 1 ? K_RED : t.k == 2 ? K_AMBER : K_BADGE;
      uint16_t fg = t.k == 3 ? K_TEXT : K_BLACK;
      G()->fillRoundRect(232 - bw, y + 3, bw, 14, 3, bg);
      text(236 - bw, y + 14, t.b, fg);
      width -= bw + 6;
    }
    bool emph = t.k == 1 || t.k == 2 || t.c == 1;
    font(emph ? u8g2_font_helvB12_te : u8g2_font_helvR12_te);
    wrap(t.t, 22, y + 15, width, 18, 2, emph ? K_WHITE : K_TEXT);
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
    font(u8g2_font_helvR08_te);
    wrap(meta, 20, y + 56, 208, 10, 1, s.x ? K_VIOLET : K_VMETA);
  }
}

static void drawUrgentCountdown(const NaggerData& d, uint32_t now) {
  G()->fillRect(0, 56, 240, 60, K_BLACK);
  uint32_t left = d.c.m.s > now ? d.c.m.s - now : 0;
  char cd[12];
  countdown(cd, sizeof(cd), left);
  font(u8g2_font_logisoso42_tn);
  textCentered(120, 108, cd, left < 60 ? K_RED : K_AMBER);
}

static void drawUrgent(const NaggerData& d, uint32_t now) {
  font(u8g2_font_helvB12_te);
  textCentered(120, 36, "SPOTKANIE ZA", K_MUTED);
  drawUrgentCountdown(d, now);
  font(u8g2_font_helvB14_te);
  wrap(d.c.m.t, 14, 150, 212, 21, 3, K_WHITE);
}

// ---- mode -----------------------------------------------------------------
void NaggerMode::begin(const Settings& s) {
  for (int i = 0; i < 60; i++) {
    float a = (i * 6 - 90) * (float)M_PI / 180.0f;
    g_secX[i] = RC_X + (int16_t)lroundf(43 * cosf(a));
    g_secY[i] = RC_Y + (int16_t)lroundf(43 * sinf(a));
    g_arcX[i] = RC_X + (int16_t)lroundf(50 * cosf(a));
    g_arcY[i] = RC_Y + (int16_t)lroundf(50 * sinf(a));
  }
  full_ = true;
  switchMs_ = millis();
}

bool NaggerMode::available(Screen sc) const {
  const NaggerData& d = naggerGet();
  switch (sc) {
    case CLOCK:    return true;
    case MEETINGS: return d.valid && (d.c.hasMeeting || d.c.nLater);
    case PROGRESS: return d.valid && d.c.nProg;
    case TODO:     return d.valid && d.c.nTodo;
    case SLACK:    return d.valid && d.c.nSlack;
    default:       return false;
  }
}

NaggerMode::Screen NaggerMode::next(Screen from) const {
  for (int hop = 1; hop <= 5; hop++) {
    Screen cand = (Screen)(((from == URGENT || from == NONE ? CLOCK : from) + hop) % 5);
    if (available(cand)) return cand;
  }
  return CLOCK;
}

void NaggerMode::drawFull(Screen sc, uint32_t now) {
  const NaggerData& d = naggerGet();
  G()->fillScreen(K_BLACK);
  switch (sc) {
    case CLOCK:    drawClock(d, now); break;
    case MEETINGS: drawMeetings(d, now); break;
    case PROGRESS: drawTasks("W TOKU", d.c.prog, d.c.nProg, d.c.nProg, 0, false); break;
    case TODO:     drawTasks("DO ZROBIENIA", d.c.todo, d.c.nTodo, d.c.totalTodo, d.c.overdue, true); break;
    case SLACK:    drawSlack(d); break;
    case URGENT:   drawUrgent(d, now); break;
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

// Once a second on the clock and countdown screens: only what moved.
void NaggerMode::tick(Screen sc, uint32_t now) {
  const NaggerData& d = naggerGet();
  if (sc == URGENT) { drawUrgentCountdown(d, now); return; }
  if (sc != CLOCK) return;
  struct tm t;
  naggerLocal(now, t);
  if (t.tm_sec == 0) for (int i = 1; i < 60; i++) drawSecondDot(i, false);
  drawSecondDot(t.tm_sec, true);
  if (t.tm_min != lastMinute_) {
    lastMinute_ = t.tm_min;
    drawClockDigits(now);
  }
  Lead l = lead(d, now);
  drawLead(l);
  if (l.arc != lastArc_) {
    lastArc_ = l.arc;
    drawArc(l.arc);
  }
  bool st = stale(d);
  if (st != lastStale_ || (st && t.tm_sec == 0)) {
    lastStale_ = st;
    drawClockBottom(d);
  }
}

void NaggerMode::service(const Settings& s) {
  const NaggerData& d = naggerGet();
  uint32_t now = naggerNow();
  uint16_t rot = d.c.rot ? d.c.rot : s.nagger.rotateSec;
  if (rot < 3) rot = 3;

  bool urgent = d.valid && d.c.hasMeeting && now && s.nagger.urgentMin &&
                now < d.c.m.s && d.c.m.s - now <= (uint32_t)s.nagger.urgentMin * 60;
  Screen want = screen_;
  if (urgent) {
    want = URGENT;
  } else if (screen_ == NONE || screen_ == URGENT || !available(screen_)) {
    want = (screen_ == NONE || screen_ == URGENT) ? CLOCK : next(screen_);
    switchMs_ = millis();
  } else if (millis() - switchMs_ >= (uint32_t)rot * 1000UL) {
    want = next(screen_);
    switchMs_ = millis();
  }

  if (want != screen_ || full_ || d.version != drawnVersion_) {
    screen_ = want;
    full_ = false;
    drawFull(screen_, now);
  } else if (now && now != lastSec_) {
    lastSec_ = now;
    tick(screen_, now);
  }
}

#endif  // WITH_NAGGER
