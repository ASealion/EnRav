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

#include "credentials.h"

#ifndef __CREDENTIALS__H
    String ssid =     "WLAN";
    String password = "xxxxxxxxxxxxxxxx";
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


int volume=15;

VS1053  Mp3Player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
MFRC522 mfrc522(MFRC522_CS, MFRC522_RST);  // Create MFRC522 instance

//----------------------------------------

typedef enum {
    CMD_UNKNOWN,
    CMD_PLAY_FILE,
    CMD_STOP, 
    CMD_VOL_UP,
    CMD_VOL_DOWN,
} PlayerCommand_e;

typedef struct {
    PlayerCommand_e Command;
    char           *pFileToPlay;
} PlayerControlMessage_s;

QueueHandle_t xPlayerQueue;
//----------------------------------------

void mp3Player( void * parameter ) {
    PlayerControlMessage_s   PlayerControlMessage;
    QueueHandle_t           *pPlayerQueue = (QueueHandle_t *) parameter;
    


    Mp3Player.begin();
    Mp3Player.setVolume(volume);
    Mp3Player.connecttoSD("/01.mp3"); // SD card

    //mp3.begin();
    //mp3.setVolume(volume);
    //mp3.connecttoSD("/01.mp3"); // SD card
    //mp3.connecttohost("edge.audio.3qsdn.com/senderkw-mp3");
    //mp3.connecttohost("https://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_fouteuur_96.mp3");

    while (1)
    {
        Mp3Player.loop();

        if( xQueueReceive( *pPlayerQueue, &(PlayerControlMessage), ( TickType_t ) 50 ) ) 
        {
            ESP_LOGV(TAG, "Received Command %u", PlayerControlMessage.Command);

            if (PlayerControlMessage.Command == CMD_PLAY_FILE) 
            {
                //make sure the file exists
                if (PlayerControlMessage.pFileToPlay != NULL)
                {
                    ESP_LOGD(TAG, "Received Path %s", PlayerControlMessage.pFileToPlay);

                    Mp3Player.connecttoSD(PlayerControlMessage.pFileToPlay); 

                    free(PlayerControlMessage.pFileToPlay);

                }
            }
            else if (PlayerControlMessage.Command == CMD_STOP) 
            {
                ESP_LOGD(TAG, "Received stop");
                Mp3Player.stop_mp3client();
            }
        }

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


    //------------------------------------------

    xPlayerQueue = xQueueCreate( 5, sizeof( PlayerControlMessage_s ) );

    if( xPlayerQueue == NULL )
    {
        ESP_LOGE(TAG, "Could not create player queue");
        while(1);
    }
    //create the task that will handle the playback
    xTaskCreate(
                    mp3Player,        /* Task function. */
                    "MP3 Player",     /* String with name of task. */
                    4* 1024,          /* Stack size in words. */
                    &xPlayerQueue,    /* Parameter passed as input of the task */
                    1,                /* Priority of the task. */
                    NULL);            /* Task handle. */

}




// The loop function is called in an endless loop
void loop()
{
    // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
    // MFRC522::MIFARE_Key key;
    // for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    delay(1000);

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


    PlayerControlMessage_s myMessage = { .Command = CMD_UNKNOWN };

    if (mfrc522.uid.size >= 2) 
    {
        if ((mfrc522.uid.uidByte[0] == 0x04) && (mfrc522.uid.uidByte[1] == 0x65))
        {
            myMessage.Command       = CMD_PLAY_FILE;
            myMessage.pFileToPlay   = (char *) malloc (16 * sizeof(char));
            strncpy(myMessage.pFileToPlay, "/01.mp3", 16); 
        }
        else if ((mfrc522.uid.uidByte[0] == 0x04) && (mfrc522.uid.uidByte[1] == 0xE8))
        {
            myMessage.Command       = CMD_PLAY_FILE;
            myMessage.pFileToPlay   = (char *) malloc (16 * sizeof(char));
            strncpy(myMessage.pFileToPlay, "/02.mp3", 16); 
        }
        else if ((mfrc522.uid.uidByte[0] == 0x04) && (mfrc522.uid.uidByte[1] == 0x01))
        {
            myMessage.Command       = CMD_STOP;
            myMessage.pFileToPlay   = NULL;
        }
    }

    if (myMessage.Command != CMD_UNKNOWN) 
    {
        if (xQueueSend( xPlayerQueue, &myMessage, ( TickType_t ) 0 ) )
        {
            ESP_LOGD(TAG, "send to queue successfull");
        } else {
            ESP_LOGD(TAG, "send to queue failed");

            //if the send failed, we must do the job
            free(myMessage.pFileToPlay);
        }
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    // PlayerControlMessage_s myMessage;

    // myMessage.Command       = CMD_PLAY_FILE;
    // myMessage.pFileToPlay   = (char *) malloc (16 * sizeof(char));
    // strncpy(myMessage.pFileToPlay, "/02.mp3", 16); 

    // if (xQueueSend( xPlayerQueue, &myMessage, ( TickType_t ) 0 ) )
    // {
    //     ESP_LOGD(TAG, "send to queue successfull");
    // } else {
    //     ESP_LOGD(TAG, "send to queue failed");

    //     //if the send failed, we must do the job
    //     free(myMessage.pFileToPlay);
    // }
}
