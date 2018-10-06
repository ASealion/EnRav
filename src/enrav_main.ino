#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"

#ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
#endif

// Digital I/O used
#define VS1053_CS     25
#define VS1053_DCS    26
#define VS1053_DREQ   32

String ssid =     "Wolles-POWERLINE";
String password = "xxxxxxxxxxxxxxxx";

int volume=20;

static const char *TAG = "main";

VS1053 mp3(VS1053_CS, VS1053_DCS, VS1053_DREQ);

//The setup function is called once at startup of the sketch
void setup() {
    
    Serial.begin(115200);
    ESP_LOGD(TAG, "Start Setup");

    pinMode(21, OUTPUT); //CS for 522
    digitalWrite(21, HIGH);

    // SPI.begin(14,2,15);
    SPI.begin(14,4,15);
    SD.begin(13);
    // WiFi.disconnect();
    // WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid.c_str(), password.c_str());
    // while (WiFi.status() != WL_CONNECTED) delay(1500);
    delay(1500);
    mp3.begin();
    mp3.setVolume(volume);
    mp3.connecttoSD("/01.mp3"); // SD card
    //mp3.connecttohost("edge.audio.3qsdn.com/senderkw-mp3");
    //mp3.connecttohost("https://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_fouteuur_96.mp3");
}

// The loop function is called in an endless loop
void loop()
{
    mp3.loop();
}

// optional:
void vs1053_info(const char *info) {                // called from vs1053
    Serial.print("DEBUG:       ");
    Serial.print(info);                             // debug infos
}

void vs1053_showstation(const char *info){          // called from vs1053
    Serial.print("STATION:     ");
    Serial.println(info);                           // Show station name
}
void vs1053_showstreamtitle(const char *info){      // called from vs1053
    Serial.print("STREAMTITLE: ");
    Serial.print(info);                             // Show title
}
void vs1053_showstreaminfo(const char *info){       // called from vs1053
    Serial.print("STREAMINFO:  ");
    Serial.print(info);                             // Show streaminfo
}
