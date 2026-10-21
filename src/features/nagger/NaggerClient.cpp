#include "config.h"
#if WITH_NAGGER
#include "NaggerClient.h"
#include "Clock.h"
#include <ArduinoJson.h>
#include <time.h>

static NaggerData g_nagger;
static uint32_t   g_hash = 0;

const NaggerData& naggerGet() { return g_nagger; }

uint32_t naggerAgeSec() {
  return g_nagger.valid ? (millis() - g_nagger.recvMs) / 1000 : UINT32_MAX;
}

uint32_t naggerNow() {
  if (clockSynced()) return (uint32_t)time(nullptr);
  if (!g_nagger.valid) return 0;
  return g_nagger.ts + (millis() - g_nagger.recvMs) / 1000;
}

void naggerLocal(uint32_t epoch, struct tm& out) {
  time_t t = (time_t)epoch;
  if (clockSynced()) { localtime_r(&t, &out); return; }
  t += g_nagger.offset;
  gmtime_r(&t, &out);
}

static void copyText(char* dst, size_t size, JsonVariantConst v) {
  strlcpy(dst, v | "", size);
}

static void readTask(NaggerTask& t, JsonObjectConst o) {
  copyText(t.t, sizeof(t.t), o["t"]);
  copyText(t.b, sizeof(t.b), o["b"]);
  t.c = o["c"] | 0;
  t.k = o["k"] | 0;
}

static void readMeeting(NaggerMeeting& m, JsonObjectConst o) {
  copyText(m.t, sizeof(m.t), o["t"]);
  m.s = o["s"] | 0u;
  m.e = o["e"] | 0u;
}

static uint32_t fnv(const void* data, size_t n) {
  const uint8_t* p = (const uint8_t*)data;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}

// {"v":1,"ts":..,"o":7200,"rot":8,"m":{"t","s","e"},"l":[..],"p":[{"t","c","b","k"}],
//  "d":[..],"nd":12,"no":2,"s":[{"t","l","a","g","x"}],"ns":6,
//  "a":{"k":"meeting|onair|urgent","t","s","e","n"}}  (Nagger's TVState)
bool naggerApply(const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  if ((doc["v"] | 0) != 1) return false;

  NaggerContent& c = g_nagger.c;
  memset(&c, 0, sizeof(c));   // deterministic bytes for the hash
  JsonObjectConst m = doc["m"];
  c.hasMeeting = !m.isNull();
  if (c.hasMeeting) readMeeting(c.m, m);
  for (JsonObjectConst o : doc["l"].as<JsonArrayConst>()) {
    if (c.nLater >= NAGGER_LATER) break;
    readMeeting(c.later[c.nLater++], o);
  }
  for (JsonObjectConst o : doc["p"].as<JsonArrayConst>()) {
    if (c.nProg >= NAGGER_TASKS) break;
    readTask(c.prog[c.nProg++], o);
  }
  for (JsonObjectConst o : doc["d"].as<JsonArrayConst>()) {
    if (c.nTodo >= NAGGER_TASKS) break;
    readTask(c.todo[c.nTodo++], o);
  }
  for (JsonObjectConst o : doc["s"].as<JsonArrayConst>()) {
    if (c.nSlack >= NAGGER_SLACK) break;
    NaggerSlack& s = c.slack[c.nSlack++];
    copyText(s.t, sizeof(s.t), o["t"]);
    copyText(s.l, sizeof(s.l), o["l"]);
    copyText(s.a, sizeof(s.a), o["a"]);
    copyText(s.g, sizeof(s.g), o["g"]);
    s.x = (o["x"] | 0) != 0;
  }
  c.totalTodo  = doc["nd"] | 0;
  c.overdue    = doc["no"] | 0;
  c.totalSlack = doc["ns"] | 0;
  c.rot        = doc["rot"] | 0;
  JsonObjectConst a = doc["a"];
  if (!a.isNull()) {
    const char* k = a["k"] | "";
    c.alert.k = !strcmp(k, "meeting") ? NAGGER_ALERT_MEETING
              : !strcmp(k, "onair")   ? NAGGER_ALERT_ONAIR
              : !strcmp(k, "urgent")  ? NAGGER_ALERT_URGENT : NAGGER_ALERT_NONE;
    copyText(c.alert.t, sizeof(c.alert.t), a["t"]);
    c.alert.s = a["s"] | 0u;
    c.alert.e = a["e"] | 0u;
    c.alert.n = a["n"] | 0;
  }

  g_nagger.ts     = doc["ts"] | 0u;
  g_nagger.offset = doc["o"] | 0;
  g_nagger.recvMs = millis();
  uint32_t h = fnv(&c, sizeof(c));
  if (!g_nagger.valid || h != g_hash) g_nagger.version++;
  g_hash = h;
  g_nagger.valid = true;
  return true;
}

#endif  // WITH_NAGGER
