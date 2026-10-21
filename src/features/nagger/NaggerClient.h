// NaggerClient.h — the Nagger push (POST /api/nagger) and the device clock it implies.
#pragma once
#include <Arduino.h>
#include "NaggerData.h"

bool              naggerApply(const String& body);   // parse a push; false on bad JSON
const NaggerData& naggerGet();
// Seconds since the last push (UINT32_MAX before the first one).
uint32_t          naggerAgeSec();
// Epoch seconds now: SNTP when it has synced, else Nagger's clock carried forward.
uint32_t          naggerNow();
// Local broken-down time for an epoch, with SNTP's TZ rule or Nagger's offset.
void              naggerLocal(uint32_t epoch, struct tm& out);
