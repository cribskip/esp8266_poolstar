/*
  ### COMMUNICATION CYCLE ###
  Heat pump sends state (even without controller)
  D1 31 04 00 00 13 13 80 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 A4 2A
  Remote answers with
  D1 31 04 00 00 13 2F 63
  Heat pump queries Remote with
  03 23 42 00 00 0E 50 53
  Remote answers with
  03 23 42 00 1C 01 00 01 00 00 00 01 5E 00 00 00 00 00 00 00 02 00 84 02 3A 00 00 00 00 00 00 FF FF 7B 27
  Heat pump sends
  D1 33 20 00 00 1B 36 01 01 01 00 00 00 02 3A 00 02 00 84 01 F4 14 0B 13 88 00 2D 00 2D 01 A8 00 00 00 00 00 00 00 00 03 20 01 7D 01 64 02 25 01 C7 01 D2 0F 14 00 BE 09 38 00 D2 02 64 7E AA
  Remote sends
  D1 33 20 00 00 1B 5D 95
  -- CYCLE REPEATS --

*/
#define XtoInt(x,y) (int16_t)word(msg[x], msg[y])

#if defined(ESP8266)
#define WIFI_SSID "" // YOUR WIFI SSID
#define WIFI_PASSWORD "" // YOUR WIFI PASSWORD
#define BROKER "" // YOUR MQTT BROKER IP

#define ON String(F("ON")).c_str()
#define OFF String(F("OFF")).c_str()

#define PUB(x,y) mqtt.publish(x, y)

#define DEBUG true

//#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#include <CRCx.h>

// ---- MQTT client --------------------------------------------------------------
#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ----             --------------------------------------------------------------
#include <SoftwareSerial.h>
SoftwareSerial PS;
#define POOLSTAR_RTS 2
#define POOLSTAR_TX 1
#define POOLSTAR_RX 3
#endif

unsigned long lastCharTime = 0;
unsigned long lastMsgTime = 0;
unsigned long nextState = 0;

struct {
  uint8_t dirty: 1;
  uint8_t sub: 2;
  uint8_t fill: 5;
} state;
uint8_t b_tmp;

#include <RingBuf.h>
RingBuf<uint8_t, 80> msg;
uint8_t m_target = 0xFF;
uint8_t m_mode = 0xFF;

#if defined(ESP8266)
void mqtt_reconnect() {
  int oldstate = mqtt.state();

  // Loop until we're reconnected
  if (!mqtt.connected()) {
    // Attempt to connect
    if (mqtt.connect(String(String("POOLSTAR") + String(millis())).c_str())) {
      mqtt.publish("POOLSTAR/node", "ON", true);
      mqtt.publish("POOLSTAR/debug", "RECONNECT");
      mqtt.publish("POOLSTAR/debug", String(millis()).c_str());
      mqtt.publish("POOLSTAR/debug", String(oldstate).c_str());

      // ... and resubscribe
      mqtt.subscribe("POOLSTAR/command/#");
      mqtt.subscribe("POOLSTAR/set/#");
    }
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void setup() {
  state.sub = 0x00;
  m_target = 30;
  m_mode = 0x80;
  state.dirty = false;

  // high speed half duplex, turn off interrupts during tx
#if defined(ESP8266)
  pinMode(POOLSTAR_RTS, OUTPUT);
  digitalWrite(POOLSTAR_RTS, LOW);
  //Serial.begin(4800);
  PS.begin(4800, SWSERIAL_8N1, POOLSTAR_RX, POOLSTAR_TX, false);
#endif

#if defined(ESP8266)
  // Init WiFi
  {
    WiFi.setOutputPower(20.5); // this sets wifi to highest power
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long timeout = millis() + 10000;

    while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
      yield();
    }

    // Reset because of no connection
    if (WiFi.status() != WL_CONNECTED) {
      //      hardreset();
    }
  }

  // Init HTTP-Update-Server
  httpUpdater.setup(&httpServer, "admin", "");
  httpServer.begin();
  MDNS.begin("POOLSTAR");
  MDNS.addService("http", "tcp", 80);

  // Init MQTT
  mqtt.setServer(BROKER, 1883);
  mqtt.setCallback(callback);
#endif
  lastCharTime = millis();

  mqtt_reconnect();
}

///////////////////////////////////////////////////////////////////////////////

// see 99_loop

///////////////////////////////////////////////////////////////////////////////

void loop() {
#if defined(ESP8266)
  if (WiFi.status() != WL_CONNECTED) ESP.restart();
  if (!mqtt.connected()) mqtt_reconnect();
  _yield();
#endif

  // Message retrival
  while (!valid_msg()) {
    _yield();
    if (PS.available()) {
      lastCharTime = millis();
      char x = PS.read();
      //msg.push(x);

      //PUB("POOLSTAR/got", String(x, HEX).c_str());

      if (msg.size() == 0) {
        if (x == 0xD1 || x == 0x03) msg.push(x);
      } else if (msg.size() > 0) {
        msg.push(x);
        if (msg.size() > 65) msg.clear();
      }
    }
  }

  _yield();

  String s;
  uint8_t sz = msg.size();
  for (uint8_t i = 0; i < sz; i++) {
    if (msg[i] < 0x10) s += "0";
    s += String(msg[i], HEX);
    s += " ";
  }

#if true
  // Output message to MQTT
  s.toUpperCase();
  PUB("POOLSTAR/msg", s.c_str());
#endif

  _yield();

  if (s.startsWith("03 2D ")) {
    uint8_t RPLY[] = {0x03, 0x2D, 0xB4, 0x00, 0x48, 0x00, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x0C, 0x01, 0xA4, 0x01, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2C, 0x00, 0x00, 0x03, 0x00, 0x00, 0xDC, 0x00, 0x14, 0x00, 0x03, 0x00, 0x00, 0x01, 0x5E, 0x01, 0x90, 0x01, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xE0, 0x00, 0x50, 0x00, 0x32, 0x01, 0x5E, 0x03, 0x34, 0x00, 0x00, 0x27, 0xFB };
    sendmsg(RPLY, sizeof(RPLY));
  } else if (s.startsWith("D1 31 ")) {
    // Extracting data
    uint8_t data[] = {msg[7], msg[8], msg[9], msg[10], msg[13], msg[23]};
    String s;
    for (uint8_t x : data) {
      s += String(x, HEX) + " ";
    }
    PUB("POOLSTAR/debug", s.c_str());

    uint8_t RPLY[] = {0xD1, 0x31, 0x04, 0x00, 0x00, 0x13, 0x2F, 0x63 };
    sendmsg(RPLY, sizeof(RPLY));
  } else if (s.startsWith("03 23 ")) {
    // Template for reply to 03 23 42 00 00 0E 50 53
    uint8_t RPLY[] = { 0x03, 0x23, 0x42, 0x00, 0x1C, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x84, 0x02, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    // Only change
    // Mode: Bytes 20 & 22
    RPLY[22] = m_mode;
    RPLY[20] = state.sub;

    // Target temperature: Bytes 23 & 24
    uint16_t tmp = m_target * 10;
    tmp += 300;

    RPLY[23] = highByte(tmp);
    RPLY[24] = lowByte(tmp);

    sendmsg(RPLY, sizeof(RPLY));
    state.dirty = false;
  } else if (s.startsWith("D1 33") && (millis() > nextState || state.dirty)) {
    // Reply if data is to be sent
    if (state.dirty) {
      PUB("POOLSTAR/debug", "DIRTY");
      uint8_t RPLY[] = { 0xD1, 0x33, 0x20, 0x00, 0x00, 0x1B };
      sendmsg(RPLY, sizeof(RPLY));
      msg.clear();
    } else {

      float tmp;

      // Print unknown values:
      /*
        uint8_t ukn[] = {
        6, 7, 8, 9, 10, 11, 12,
        15,
        17,
        19,
        21, 22, 23, 24,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38
        };
        for (uint8_t i : ukn) {
        if (msg[i] < 0x10) s += "0";
        s += String(msg[i], HEX);
        s += " ";
        }
        s.toUpperCase();
        PUB("POOLSTAR/unknown", s.c_str());*/

      // Identified:
      // T1, T2, T3, T4, T6, T7, T8, Ft, Fr, Pr, dcU, dcC, AcU, AcC
      // To be resolved:
      // T5, 1F, 2F, od, dF, OIL, r2, STF, Pu, AH, Ad, AL,

      // Target temperature
      m_target = getFloat(13);
      PUB("POOLSTAR/TARGET", String(m_target).c_str());

      // T1
      PUB("POOLSTAR/T1", String(getFloat(39)).c_str());
      mqtt.loop();

      // T2
      PUB("POOLSTAR/T2", String(getFloat(41)).c_str());

      // T3 / INLET
      PUB("POOLSTAR/T3", String(getFloat(47)).c_str());

      // T4 / OUTLET
      PUB("POOLSTAR/T4", String(getFloat(49)).c_str());

      // T6 / OUTDOOR
      PUB("POOLSTAR/T6", String(getFloat(43)).c_str());

      // T7
      PUB("POOLSTAR/T7", String(getFloat(59)).c_str());

      // T8
      PUB("POOLSTAR/T8", String(getFloat(45)).c_str());
      mqtt.loop();

      // Ft
      PUB("POOLSTAR/Ft", String(XtoInt(25, 26)).c_str());

      // Fr
      PUB("POOLSTAR/Fr", String(XtoInt(27, 28)).c_str());

      // Pr
      PUB("POOLSTAR/Pr", String(XtoInt(19, 20)).c_str());

      // DcV
      tmp = msg[51] * 25;
      tmp += msg[52] / 10;
      PUB("POOLSTAR/DcV", String(tmp).c_str());

      // DcC
      tmp = msg[53] * 2;
      tmp += msg[54] / 100;
      PUB("POOLSTAR/DcC", String(tmp).c_str());

      // AcV * AcC = W
      tmp = word(msg[55], msg[56]);
      tmp *= word(msg[57], msg[58]);
      tmp /= 1000;
      PUB("POOLSTAR/W", String(tmp).c_str());
      mqtt.loop();

      // Mode and Submode ----------------------------------------------
      String mode;
      m_mode = msg[18];
      if (m_mode & 0x80) {
        if (m_mode & 0x04) mode = "Heat";
        else if (m_mode & 0x01) mode = "Cool";
        else mode = "Auto";
        if (mode.length() > 2) PUB("POOLSTAR/MODE", mode.c_str());
        _yield();

        mode = "";
        b_tmp = msg[16];
        state.sub = b_tmp;
        if (b_tmp == 0x02) mode = "Eco";
        else if (b_tmp == 0x01) mode = "Boost";
        else mode = "Auto";
        if (mode.length()) PUB("POOLSTAR/SUB", mode.c_str());
      } else {
        mode = "Off";
        PUB("POOLSTAR/MODE", mode.c_str());
      }
      mqtt.loop();

      nextState = millis() + 5000;
    }
  }

  msg.clear();
}
