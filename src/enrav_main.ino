#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"
#include <MFRC522.h>

#ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
#else
    static const char *TAG = "main";
#endif

// Digital I/O used
#define SPI_CLK         14    
#define SPI_MISO         4
#define SPI_MOSI        15

#define SDCARD_CS       13

#define VS1053_CS       25
#define VS1053_DCS      26
#define VS1053_DREQ     32

#define MFRC522_RST     19
#define MFRC522_CS      21

String ssid =     "Wolles-POWERLINE";
String password = "xxxxxxxxxxxxxxxx";

int volume=10;

VS1053  mp3(VS1053_CS, VS1053_DCS, VS1053_DREQ);
MFRC522 mfrc522(MFRC522_CS, MFRC522_RST);  // Create MFRC522 instance


void mp3Player( void * parameter ) {
    VS1053 *pPlayer = (VS1053 *) parameter;

    pPlayer->begin();
    pPlayer->setVolume(volume);
    pPlayer->connecttoSD("/01.mp3"); // SD card

    //mp3.begin();
    //mp3.setVolume(volume);
    //mp3.connecttoSD("/01.mp3"); // SD card
    //mp3.connecttohost("edge.audio.3qsdn.com/senderkw-mp3");
    //mp3.connecttohost("https://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_fouteuur_96.mp3");

    while (1)
    {
        pPlayer->loop();

        delay(50);
    }
}

//The setup function is called once at startup of the sketch
void setup() {
    
    Serial.begin(115200);

    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI);

    mfrc522.PCD_Init();		            // Init MFRC522
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
	ESP_LOGD(TAG, "Scan PICC to see UID, SAK, type, and data blocks...     ");

    SD.begin(SDCARD_CS);

    // WiFi.disconnect();
    // WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid.c_str(), password.c_str());
    // while (WiFi.status() != WL_CONNECTED) delay(1500);
    delay(1500);


    //create the task that will handle the playback
    xTaskCreate(
                    mp3Player,        /* Task function. */
                    "MP3 Player",     /* String with name of task. */
                    4* 1024,          /* Stack size in words. */
                    &mp3,             /* Parameter passed as input of the task */
                    1,                /* Priority of the task. */
                    NULL);            /* Task handle. */


}

// The loop function is called in an endless loop
void loop()
{

    // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    // Look for new cards
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
        return;
    }

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    Serial.print(F("Card UID:"));    //Dump UID
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
}
