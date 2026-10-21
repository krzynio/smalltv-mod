// loader.cpp — a minimal OTA "trampoline" for devices whose stock firmware
// reserves so much flash that its updater rejects a full smalltv-mod image
// ("Not Enough Space"), e.g. the GeekMagic SmallTV-ultra with its large
// image/GIF store.
//
// It is deliberately tiny: WiFi + a bare web OTA endpoint, no display, no
// features. Small enough to fit the stock firmware's slot, it is flashed over
// the stock OTA. Once running it uses smalltv-mod's normal 4m1m flash layout,
// whose sketch region is large, so ITS /update then accepts the full firmware
// the stock updater refused. Two hops, no UART needed.
//
// WiFi creds are compile-time only (-DLOADER_SSID / -DLOADER_PASS, or
// DEFAULT_WIFI_SSID in the untracked src/secrets.h), never in the repo; then the
// network the stock firmware saved. Without any it opens an open "GIFTV" AP at
// 192.168.4.1 so the full image can still be uploaded by hand.
#ifdef LOADER_BUILD
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include "StockPages.h"

// The local default network (src/secrets.h, untracked) doubles as the loader's.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#if !defined(LOADER_SSID) && defined(DEFAULT_WIFI_SSID)
#define LOADER_SSID DEFAULT_WIFI_SSID
#define LOADER_PASS DEFAULT_WIFI_PASS
#endif
#ifndef LOADER_SSID
#define LOADER_SSID ""
#endif
#ifndef LOADER_PASS
#define LOADER_PASS ""
#endif

static ESP8266WebServer       server(80);
static ESP8266HTTPUpdateServer updater;

static bool waitJoined() {
  for (int i = 0; i < 80 && WiFi.status() != WL_CONNECTED; i++) { delay(250); yield(); }
  return WiFi.status() == WL_CONNECTED;
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\nsmalltv-mod loader"));
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("giftv-loader");

  // The network from the build, then the one the stock firmware left in the SDK's
  // saved config (the device was on Wi-Fi a moment ago, under the stock image).
  bool joined = false;
  if (LOADER_SSID[0]) {
    WiFi.begin(LOADER_SSID, LOADER_PASS);
    joined = waitJoined();
  }
  if (!joined) {
    WiFi.begin();
    joined = waitJoined();
  }
  if (!joined) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GIFTV");                 // open AP -> browse to 192.168.4.1/update
  }

  Serial.println(joined ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
  // http://giftv-loader.local/update: the screen stays dark in the loader, so the
  // name is how to find it after the stock updater reboots into it.
  if (joined) MDNS.begin("giftv-loader");
  // The stock-looking form first (the server takes the first matching handler),
  // the library's POST handler does the flashing.
  server.on("/update", HTTP_GET, []() { server.send_P(200, "text/html", STOCK_UPDATE_PAGE); });
  server.on("/", HTTP_GET, []() { server.sendHeader("Location", "/update"); server.send(302, "text/plain", ""); });
  updater.setup(&server);
  server.begin();
}

void loop() {
  server.handleClient();
  MDNS.update();
  yield();
}
#endif  // LOADER_BUILD
