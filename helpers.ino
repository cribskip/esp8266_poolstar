#include <CRCx.h>

void _yield() {
  httpServer.handleClient();
  yield();
  mqtt.loop();
}

uint16_t calcCRC(uint8_t * arr, uint8_t size) {
  uint16_t CRC = crcx::crc16(arr, size);
  return CRC;
}

bool valid_msg() {
  if (msg.size() < 8) return false;
  
  // Copy buffer
  uint8_t sz = msg.size() - 2;
  uint8_t tmp[sz];
  for (uint8_t i = 0; i < sz; i++) {
    tmp[i] = msg[i];
  }

#if false
  uint16_t CRC = crcx::crc16(tmp, sizeof(tmp));
  if (CRC == 0x00) return true;
  else false;

#else
  // Calculate and extract CRCs
  uint16_t CRC = crcx::crc16(tmp, sizeof(tmp));
  uint16_t msgCRC = word(msg[msg.size() - 1], msg[msg.size() - 2]);

  // Compare and return
  if (CRC == msgCRC) return true;
  else return false;
#endif
}

float getFloat(byte x) {
  float tmp;
  tmp = word(msg[x], msg[x + 1]);
  tmp -= 300;
  tmp /= 10;
  return tmp;
}

void sendmsg(uint8_t * message, uint8_t len) {
  String s;
  uint8_t x;
  uint16_t CRC = calcCRC(message, len);
  digitalWrite(POOLSTAR_RTS, HIGH);
  while (millis() < (lastCharTime + 19)) _yield();
  //delay(1);
  PS.flush();
  yield();

  for (uint8_t i = 0; i < len; i++) {
    x = message[i];
    if (x < 0x10) s += "0";
    s += String(x, HEX);
    s += " ";
    PS.write(x);
    PS.flush();
    yield();
  }
  PS.write(lowByte(CRC));
  PS.write(highByte(CRC));
  yield();
  PS.flush();
  digitalWrite(POOLSTAR_RTS, LOW);

  PUB("POOLSTAR/msg", s.c_str());
  _yield();
}
