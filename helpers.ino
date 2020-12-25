#include <CRCx.h>

bool valid_msg() {
  // Copy buffer
  uint8_t sz = msg.size() - 2;
  uint8_t tmp[sz];
  for (uint8_t i = 0; i < sz; i++) {
    tmp[i] = msg[i];
  }

  // Calculate and extract CRCs
  uint16_t CRC = crcx::crc16(tmp, sizeof(tmp));
  uint16_t msgCRC = word(msg[msg.size() - 1], msg[msg.size() - 2]);


  // Compare and return
  if (CRC == msgCRC) return true;
  else {
    Serial.println(CRC, HEX);
    Serial.println(msgCRC, HEX);
    return false;
  }
}

float getFloat(byte x) {
  float tmp;
  tmp = word(msg[x], msg[x + 1]);
  tmp -= 300;
  tmp /= 10;
  return tmp;
}
