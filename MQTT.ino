// function called when a MQTT message arrived
void callback(char* p_topic, byte * p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  String topic = String(p_topic);

  mqtt.publish("POOLSTAR/debug", topic.c_str());
  _yield();

  // handle message topic
  if (topic.startsWith("POOLSTAR/command/")) {
    if (topic.endsWith("reset")) ESP.restart();
  } else if (topic.startsWith("POOLSTAR/set/")) {
    if (topic.endsWith("target")) {
      m_target = payload.toInt();
      state.dirty = true;
    } else if (topic.endsWith("mode")) {
      if (payload.equals("OFF")) m_mode = 0x00;
      else if (payload.equals("Heat")) m_mode = 0x84;
      else if (payload.equals("Cool")) m_mode = 0x81;
      else if (payload.equals("Auto")) m_mode = 0x88;
      else if (payload.equals("ON")) m_mode = 0x88;
      state.sub = 0x02;
      state.dirty = true;
    }  else if (topic.endsWith("sub")) {
      if (payload.equals("Auto")) state.sub = 0x00;
      else if (payload.equals("Boost")) state.sub = 0x01;
      else if (payload.equals("Eco")) state.sub = 0x02;
      state.dirty = true;
    }
  }
}
