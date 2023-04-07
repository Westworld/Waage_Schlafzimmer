void UDBDebug(String message);
void MQTT_Send(char const * topic, long value);
void MQTT_Send(char const * topic, int16_t value);
void MQTT_Send(char const * topic, float value);
void MQTT_Send(char const * topic, String value);
void MQTTcallback(char* topic, byte* payload, unsigned int length);
void SendeString(String url);
void SendeLicht(int licht);
void SendeStatus(float Gewicht, int warum, float Gelesen);
void WIFI_Connect();