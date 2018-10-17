#include "UserInterface.h"

#ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
#else
    static const char *TAG = "UserInterface";
#endif


UserInterface::UserInterface(uint8_t _cs_pin = 21, uint8_t _rst_pin = 19)
{
    m_pRfReader = new MFRC522(_cs_pin, _rst_pin);
}

UserInterface::~UserInterface()
{
}


void UserInterface::begin( void )
{
    ESP_LOGD(TAG, "Start User Interface Task");

    m_pRfReader->PCD_Init();		            // Init MFRC522
	m_pRfReader->PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
	ESP_LOGD(TAG, "Scan PICC to see UID, SAK, type, and data blocks...     ");

    //create the task that will handle the playback
    xTaskCreate(
                    TaskFunctionAdapter,        /* Task function. */
                    "UserInterface",       	    /* String with name of task. */
                    4* 1024,                    /* Stack size in words. */
                    this,                       /* Parameter passed as input of the task */
                    1,                          /* Priority of the task. */
                    &m_handle);                 /* Task handle. */

}


void UserInterface::TaskFunctionAdapter(void *pvParameters)
{
    UserInterface *userInterface = static_cast<UserInterface *>(pvParameters);

    userInterface->Run();

    userInterface->CleanUp();

    vTaskDelete(userInterface->m_handle);
}



void UserInterface::Run( void ) {
    
    ESP_LOGD(TAG, "Thread started");

    while (true)
    {

        // Look for new cards
        if ( m_pRfReader->PICC_IsNewCardPresent() == true) {

            ESP_LOGD(TAG, "New card detected");

            // Select one of the cards
            if ( m_pRfReader->PICC_ReadCardSerial()) {
            
                // Show some details of the PICC (that is: the tag/card)
                ESP_LOGD(TAG, "Card UID:");
                dump_byte_array(m_pRfReader->uid.uidByte, m_pRfReader->uid.size);

                Serial.println();
                Serial.print(F("PICC type: "));
                MFRC522::PICC_Type piccType = m_pRfReader->PICC_GetType(m_pRfReader->uid.sak);
                Serial.println(m_pRfReader->PICC_GetTypeName(piccType));


                //end communication with the card
                m_pRfReader->PICC_HaltA();
                m_pRfReader->PCD_StopCrypto1();
            }
        }

        delay(20);
    };
}


void UserInterface::dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void UserInterface::CleanUp( void )
{

}

void UserInterface::setQueue( QueueHandle_t *pQueue )
{
    this->m_pPlayerQueue = pQueue;
}