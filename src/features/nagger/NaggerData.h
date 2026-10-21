// NaggerData.h — what Nagger last pushed (POST /api/nagger), fixed-size so the
// ESP8266 heap never sees it. Texts are UTF-8 and already cut by Nagger to fit.
#pragma once
#include <Arduino.h>

#define NAGGER_TEXT    104   // Nagger cuts texts to 100 UTF-8 bytes
#define NAGGER_TASKS     4
#define NAGGER_SLACK     3
#define NAGGER_LATER     3

struct NaggerMeeting {
  char     t[NAGGER_TEXT];
  uint32_t s;            // start, epoch seconds
  uint32_t e;            // end
};

struct NaggerTask {
  char    t[NAGGER_TEXT];
  char    b[18];         // badge text ("wczoraj", "dziś", "22.10")
  uint8_t c;             // priority: 1 urgent, 2 high, 3 medium, 4 low, 0 none
  uint8_t k;             // badge kind: 1 overdue, 2 today, 3 later, 0 none
};

struct NaggerSlack {
  char t[NAGGER_TEXT];
  char l[26];            // "do odpisania", "prośba"
  char a[34];            // author
  char g[14];            // age ("5 h")
  bool x;                // escalated
};

// What Nagger is nagging about on the Mac right now (Guru, takeover, ON AIR):
// the device drops the rotation for a blinking full screen.
#define NAGGER_ALERT_NONE     0
#define NAGGER_ALERT_MEETING  1   // starts soon
#define NAGGER_ALERT_ONAIR    2   // just started
#define NAGGER_ALERT_URGENT   3   // new urgent task

struct NaggerAlert {
  uint8_t  k;
  char     t[NAGGER_TEXT];
  uint32_t s;            // meeting start / end (epoch seconds)
  uint32_t e;
  uint16_t n;            // more urgent tasks behind this one
};

// Everything that is drawn; hashed to tell a real change from a heartbeat push.
struct NaggerContent {
  bool          hasMeeting;
  NaggerMeeting m;
  NaggerMeeting later[NAGGER_LATER];
  uint8_t       nLater;
  NaggerTask    prog[NAGGER_TASKS];
  uint8_t       nProg;
  NaggerTask    todo[NAGGER_TASKS];
  uint8_t       nTodo;
  uint16_t      totalTodo;
  uint16_t      overdue;
  NaggerSlack   slack[NAGGER_SLACK];
  uint8_t       nSlack;
  uint16_t      totalSlack;
  uint16_t      rot;      // seconds per screen; 0 = the device setting
  NaggerAlert   alert;
};

struct NaggerData {
  NaggerContent c;
  bool     valid = false;
  uint32_t recvMs = 0;    // millis() of the last push
  uint32_t ts = 0;        // Nagger's clock at that push (epoch seconds)
  int32_t  offset = 0;    // Nagger's UTC offset (s), for before SNTP syncs
  uint32_t version = 0;   // bumps when the content changes
};
