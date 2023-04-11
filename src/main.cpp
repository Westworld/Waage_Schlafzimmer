
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "FastLED.h"
#include <DebounceEvent.h>
#include <PubSubClient.h>

#include "HX711.h"

#include "main.h"

#define Katze "Timmi"

float KatzeGewichtStart = 8.0;
float KatzeGewichtEnde = 8.9;

#define UDPDEBUG 1
#ifdef UDPDEBUG
WiFiUDP udp;
const char * udpAddress = "192.168.0.34";
const int udpPort = 19814;
#endif

const char* MQTT_BROKER = "192.168.0.46";
 
WiFiClient espClient;
PubSubClient client(espClient);

const char* host = "192.168.0.59"; // "192.168.0.34";
const int httpPort = 80;

uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = D6; //A2;
const int LOADCELL_SCK_PIN = D5; // A3;
const float kalibrierung = -21600.F;

#define DATA_PIN D1 //22
#define NUM_LEDS 3
CRGB leds[NUM_LEDS];

int counter=2000, ledcounter=0, ledjob=0, timecounter=0;

#define HOST_NAME Katze

char logString[200];
float Gewicht=0;
float AltGewicht=0;
int Messungen=0;
int TaraCounter = 0;

float GewichtMittel[10];
int GewichtAnzahl=0;

int LichtBad=0;
int LichtDach=0;
int LichtGang=0;

HX711 scale;

#define CUSTOM_DEBOUNCE_DELAY   50
// Time the library waits for a second (or more) clicks
// Set to 0 to disable double clicks but get a faster response
#define CUSTOM_REPEAT_DELAY     500

DebounceEvent * button1;
DebounceEvent * button2;
DebounceEvent * button3;
DebounceEvent * button4;


void setup() {
  // put your setup code here, to run once:
 
   Serial.begin(115200);
   Serial.println("Start");
   FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
   leds[0] = CRGB(0, 0, 0);  // black
   leds[1] = CRGB(0, 0, 0);  // black
   leds[2] = CRGB(0, 0, 0);  // black
   FastLED.show();
   leds[0] = CRGB(0, 255, 0);  // green
   FastLED.show();

   WIFI_Connect();
 
   client.setServer(MQTT_BROKER, 1883);
   client.setCallback(MQTTcallback);

   delay(1000);
   leds[0] = CRGB(0, 0, 0);  // black
   FastLED.show();

button1 = new DebounceEvent(D4, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, CUSTOM_DEBOUNCE_DELAY, CUSTOM_REPEAT_DELAY);
button2 = new DebounceEvent(D3, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, CUSTOM_DEBOUNCE_DELAY, CUSTOM_REPEAT_DELAY);
button3 = new DebounceEvent(D2, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, CUSTOM_DEBOUNCE_DELAY, CUSTOM_REPEAT_DELAY);
button4 = new DebounceEvent(D7, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, CUSTOM_DEBOUNCE_DELAY, CUSTOM_REPEAT_DELAY);
  
  ArduinoOTA.setHostname(Katze);
  
  ArduinoOTA
    .onStart([]() {
     });
    ArduinoOTA.onEnd([]() {
     });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
     });
    ArduinoOTA.onError([](ota_error_t error) {
      /*
      debugV("Error[%d]", error);
      if (error == OTA_AUTH_ERROR)  rdebugVln("Auth Failed");
      else if (error == OTA_BEGIN_ERROR)  rdebugVln("Begin Failed");
      else if (error == OTA_CONNECT_ERROR)  rdebugVln("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR)  rdebugVln("Receive Failed");
      else if (error == OTA_END_ERROR)  rdebugVln("End Failed");
      */
    });

  ArduinoOTA.begin();  

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(kalibrierung); // (223.F);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare(20);                // reset the scale to 0
  Serial.println("begin");
  delay(500);
}


void loop_test() { 
                leds[1] = CRGB::Red; FastLED.show(); delay(1000); 
                leds[1] = CRGB::Black; FastLED.show(); delay(1000);
        }


void loop() {

  if (WiFi.status() != WL_CONNECTED)
    {
      WIFI_Connect();
    }
  
  ArduinoOTA.handle();


  if (!client.connected()) {
        while (!client.connected()) {
            client.connect(Katze,MQTT_User,MQTT_Pass);
            client.subscribe("display/leds");
            // subscribe Licht Dach, Bad
            client.subscribe("hm/status/Dach/STATE");
            client.subscribe("hm/status/Bad/STATE");
            client.subscribe("hm/status/Gang/STATE");
            delay(100);
            Serial.println("reconnect mqtt");
        }
    }
    client.loop();
    

  if (unsigned int event = button1->loop()) {
        if (event == EVENT_RELEASED) {
          Serial.println("button 1");
          UDBDebug("button 1");
             SendeLicht(1);
        }
  }

  if (unsigned int event = button2->loop()) {
        if (event == EVENT_RELEASED) {
          Serial.println("button 2");
          UDBDebug("button 2");
                     SendeLicht(2);
        }
  }
  
  if (unsigned int event = button3->loop()) {
        if (event == EVENT_RELEASED) {
                    UDBDebug("button 3");
             SendeLicht(3);
        }
  }
  
  if (unsigned int event = button4->loop()) {
        if (event == EVENT_RELEASED) {
                    UDBDebug("button 4");
             SendeLicht(4);
        }
  }
    
  
  if (counter++>2000) 
  {

    counter=0;
    // 2 Sekunden pro Vorgang
    if (ledjob == 0) {
       leds[0] = CRGB::Black;
       FastLED.show();
    }
   //Serial.println("counter>2000");
   if (ledcounter == 0) 
      ledcounter=1;
    else
      ledcounter=0;
  }  // counter>2000
  else {
    // Vorgang läuft noch
    if (ledcounter == 0) {
        switch (ledjob) {
          case 1:
          case 4:
            leds[0] = CHSV(0, 255, map(counter,0,2000,64,255)); //int(counter/8));
            break;
          case 2:
          case 5:
            leds[0] = CHSV(64, 255, map(counter,0,2000,64,255)); //int(counter/8));
            break;
          case 3:
          case 6:
            leds[0] = CHSV(map(counter,0,2000,0,64), 255, map(counter,0,2000,64,192));  // bis 64 (int(counter/32)
            break;
        }
    }
    else  // ledcounter#0
    {    
      switch (ledjob) {
        case 1:
        case 4:
          leds[0] = CHSV(0, 255, map(counter,2000,0,64,255)); //int((2000-counter)/8));
          break;
        case 2:
        case 5:
          leds[0] = CHSV(64, 255, map(counter,2000,0,64,255));
          break;
        case 3:
        case 6:
          leds[0] = CHSV( map(counter,0,2000,64,0), 255,map(counter,2000,0,64,192));
          break;
         }
     }
     if (ledjob > 3) counter++;  // doppeltes Tempo
     if (ledjob != 0)
          FastLED.show();
  }

  float Gelesen=0;

 if ((counter == 1000)  | (counter == 1999)) {
  // alle 1 sec
    Gelesen = scale.get_units(10);
    if ((Gelesen <= (AltGewicht + 0.1)) && (Gelesen >= (AltGewicht - 0.1)) ) {  // gleicher Wert erneut gelesen     
      if (!((Gelesen <= (Gewicht + 0.1)) && (Gelesen >= (Gewicht - 0.1)) )) {  // Wert noch nicht gesendet
              Gewicht = Gelesen;
              SendeStatus(Gelesen, 1, Gelesen); 
              TaraCounter = 0;
      }
    }  
    else
      AltGewicht = Gelesen;

     if ((Gelesen <= 2.0) && (Gelesen >= -4.8 ))    
     {
      if (++TaraCounter > 1800) {
        TaraCounter = 0;
        scale.tare(20);  
        Gelesen = scale.get_units(10);
        SendeStatus(Gewicht, 3, Gelesen);
       }

    Messungen++;

      if (Messungen > 30) {
        // eine Minute
        if (Gewicht > 5)
          SendeStatus(Gewicht, 0, Gelesen);
        Messungen = 0;
      }
  }
     
   } // alle  1 sec

  delay(1);

}




// #############################################################
void WIFI_Connect()
{
  WiFi.disconnect();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_STA);
  WiFi.hostname(Katze);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
    // Wait for connection
  for (int i = 0; i < 25; i++)
  {
    if ( WiFi.status() != WL_CONNECTED ) {
      Serial.print ( "." );
      delay ( 500 );
    }
    else
    {  Serial.println("connected");
       return; }
  }

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

}

// #############################################################

void SendeStatus(float Gewicht, int warum, float Gelesen) {

  float sende = roundf(Gewicht * 89) / 100;   // Waage * 0.89 zur Korrektur

  if (Gewicht > 3) {
    UDBDebug("SendeStatus Timmi "+String(Gewicht));
    MQTT_Send("display/Gewicht", sende);
  }
  
  if ((sende > KatzeGewichtStart) && (sende < KatzeGewichtEnde)) {
    UDBDebug("Timmi gewogen: "+String(sende));
    sende = BerechneDurchschnitt(Gewicht);
    sende = roundf(sende * 100) / 100;
    UDBDebug("Timmi Durchschnitt: "+String(sende));
    MQTT_Send("HomeServer/Tiere/Timmi", sende);
  }  
}

void SendeLicht(int licht) {
     Serial.print("SendeLicht ");
     Serial.println(licht);
     
   leds[0] = CRGB(0, 255, 0);  // black
   FastLED.show();
   delay(1000);

  UDBDebug("Licht: "+String(licht));

  switch (licht) {
    case 1:
      if (LichtDach == 1)
        MQTT_Send("hm/set/Dach/STATE", 0L);
      else
        MQTT_Send("hm/set/Dach/STATE", 1L);
      break;
    case 2:
      if (LichtBad == 2)
        MQTT_Send("hm/set/Bad/STATE", 0L);
      else
        MQTT_Send("hm/set/Bad/STATE", 1L);
      break;
    case 3:
      if (LichtGang == 3)
        MQTT_Send("hm/set/Gang/STATE", 0L);
      else
        MQTT_Send("hm/set/Gang/STATE", 1L);
      break; 
    case 4:
        MQTT_Send("hm/set/Lichtwarner_SZ/STATE", 0L);
      break;
   }  
   leds[0] = CRGB(0, 0, 0);  // black
   FastLED.show();
}

float BerechneDurchschnitt(float neu) {
  if (GewichtAnzahl>9) {
    for (int i = 0; i<9; i++) {
      GewichtMittel[i] = GewichtMittel[i+1];
    }
    GewichtAnzahl--;
  }

  GewichtMittel[GewichtAnzahl++] = neu;

  float mittel = 0;
  for (int i = 0; i<10; i++) {
      mittel += GewichtMittel[i];
  }
  return (mittel / GewichtAnzahl);  
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {

   if(length==1) {
      String stopic = String(topic);
      if (stopic == "hm/status/Bad/STATE") {
        short state=0;
        state = payload[0]-48; 
        LichtBad = state;
        UDBDebug(stopic+" - "+String(state));
        return;
      }
      if (stopic == "hm/status/Dach/STATE") {
        short state=0;
        state = payload[0]-48; 
        LichtDach = state;
        UDBDebug(stopic+" - "+String(state));
        return;
      }
      if (stopic == "hm/status/Gang/STATE") {
        short state=0;
        state = payload[0]-48; 
        LichtGang = state;
        UDBDebug(stopic+" - "+String(state));
        return;
      }
   }

    if(length==1) {
      ledjob = payload[0]-48;  // job as ascii
      if (ledjob != 0)
        UDBDebug("Lichtjob: "+String(ledjob));
      //debugV("MQTT message new led job = %d", ledjob);
      counter=2000, ledcounter=0; // reset
      leds[0] = CRGB::Black;
      FastLED.show();
      /*
        0 = black
        1 = red
        2 = yellow
        3 = red<->yellow
        4 = red fast
        5 = yellow fast
        6 = red<->yellow fast
       */
       Serial.print("mqtt: ");
       Serial.println(ledjob);
    }
    else
      Serial.println("empty MQTT message");
}


void MQTT_Send(char const * topic, String value) {
    //Serial.println("MQTT " +String(topic)+" "+value) ;
    if (!client.publish(topic, value.c_str(), true)) {
       UDBDebug("MQTT error");  
    };
      UDBDebug(String(topic)+" - "+value);
}

void MQTT_Send(char const * topic, float value) {
    char buffer[10];
    snprintf(buffer, 10, "%f", value);
    MQTT_Send(topic, buffer);
}

void MQTT_Send(char const * topic, int16_t value) {
    char buffer[10];
    snprintf(buffer, 10, "%d", value);
    MQTT_Send(topic, buffer);
}

void MQTT_Send(char const * topic, long value) {
    char buffer[10];
    snprintf(buffer, 10, "%ld", value);
    MQTT_Send(topic, buffer);
}

void UDBDebug(String message) {
#ifdef UDPDEBUG
  udp.beginPacket(udpAddress, udpPort);
  udp.write((const uint8_t* ) message.c_str(), (size_t) message.length());
  udp.endPacket();
#endif  
}