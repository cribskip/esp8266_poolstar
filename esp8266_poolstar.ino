
#define XtoInt(x,y) (int16_t)word(msg[x], msg[y])


#if defined(ESP8266)
#define WIFI_SSID "" // YOUR WIFI SSID
#define WIFI_PASSWORD "" // YOUR WIFI PASSWORD
#define BROKER "" // YOUR MQTT BROKER IP

#define ON String(F("ON")).c_str()
#define OFF String(F("OFF")).c_str()

#define PUB(x,y) mqtt.publish(x, y)

#define DEBUG true

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// ---- MQTT client --------------------------------------------------------------
#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ----             --------------------------------------------------------------
#include <SoftwareSerial.h>
#define POOLSTAR_TXRX 3
#define POOLSTAR_RTS 2
SoftwareSerial Poolstar;
#endif

unsigned long lastCharTime = 0;
unsigned long lastMsgTime = 0;
unsigned long nextState = 0;
struct {
  uint8_t sent: 1;
  uint8_t do_parse: 1;
} state;

uint8_t b_tmp;

#include <RingBuf.h>
RingBuf<uint8_t, 80> msg;

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
      //mqtt.subscribe("POOLSTAR/command/#");
      //mqtt.subscribe("POOLSTAR/set/#");
    }
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void setup() {
  // high speed half duplex, turn off interrupts during tx
#if defined(ESP8266)
  Poolstar.begin(4800, SWSERIAL_8N1, POOLSTAR_TXRX, POOLSTAR_TXRX, false, 256);
  Poolstar.enableIntTx(false);
#else
  Serial.begin(4800);
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
  //mqtt.setCallback(callback);
#endif
}

///////////////////////////////////////////////////////////////////////////////

// see 99_loop

///////////////////////////////////////////////////////////////////////////////

void loop() {

#if defined(ESP8266)
  if (WiFi.status() != WL_CONNECTED) ESP.restart();
  if (!mqtt.connected()) mqtt_reconnect();
  httpServer.handleClient();
  yield();
  mqtt.loop();
#endif

  // Message retrival
  if (Poolstar.available()) {
    char x = Poolstar.read();
    lastCharTime = millis();
    msg.push(x);
  }

  httpServer.handleClient();
  yield();
  mqtt.loop();

  // Output when 20ms gap and got message
  if (lastCharTime + 20 < millis() && msg.size() > 5) {
    String s;
    uint8_t sz = msg.size();

    if (sz > 8 && valid_msg()) {
      for (uint8_t i = 0; i < sz; i++) {
        if (msg[i] < 0x10) s += "0";
        s += String(msg[i], HEX);
        s += " ";
      }
      s.toUpperCase();
      s = s.substring(0, s.length() - 6);

      if (s.startsWith("03 2D ")) {
        s.clear();
      } else if (s.startsWith("03 23 42 00 1C")) {
        PUB("POOLSTAR/debug", s.c_str());
      } else if (s.startsWith("D1 33 20 00 00 1B") && millis() > nextState) {
        //if (s.length() >  24) { //&& millis() > nextState || s.startsWith("")) {
        PUB("POOLSTAR/debug", s.c_str());
        float tmp;

        // OK
        PUB("POOLSTAR/TARGET", String(getFloat(13)).c_str());

        // T1
        PUB("POOLSTAR/T1", String(getFloat(39)).c_str());

        // T2
        PUB("POOLSTAR/T2", String(getFloat(41)).c_str());

        // T3
        PUB("POOLSTAR/IN", String(getFloat(47)).c_str());

        // T4
        PUB("POOLSTAR/OUT", String(getFloat(49)).c_str());

        // T6
        PUB("POOLSTAR/OUTDOOR", String(getFloat(43)).c_str());

        // T8
        PUB("POOLSTAR/T8", String(getFloat(45)).c_str());

        // Ft
        PUB("POOLSTAR/Ft", String(XtoInt(25, 26)).c_str());

        // Fr
        PUB("POOLSTAR/Fc", String(XtoInt(27, 28)).c_str());

        if (word(msg[19], msg[20]) != 0)
          PUB("POOLSTAR/UNKN0", String(getFloat(19)).c_str());

        // UNKNOWN
#if false
        PUB("POOLSTAR/UNKNA", String(XtoInt(21, 22)).c_str());
        PUB("POOLSTAR/UNKNB", String(XtoInt(23, 24)).c_str());

        PUB("POOLSTAR/UNKN6", String(XtoInt(51, 52)).c_str());
        PUB("POOLSTAR/UNKN7", String(XtoInt(53, 54)).c_str());
        PUB("POOLSTAR/UNKN8", String(XtoInt(59, 60)).c_str());
#endif

        String mode;
        b_tmp = msg[18];
        if (b_tmp = 0x00) mode = "OFF";
        else {
          switch (b_tmp) {
            case 0x88: mode = "AUTO"; break;
            case 0x82: mode = "BoostHeat"; break;
            case 0x84: mode = "SilentHeat"; break;
          }
        }
        if (mode.length() > 2) PUB("POOLSTAR/MODE", mode.c_str());

        tmp = word(msg[55], msg[56]);
        tmp *= word(msg[57], msg[58]);
        tmp /= 1000;
        PUB("POOLSTAR/W", String(tmp).c_str());

        nextState = millis() + 5000;
      } else {
        PUB("POOLSTAR/debug", s.c_str());
      }
    }
    //Serial.println(s);
    msg.clear();
  }
}
