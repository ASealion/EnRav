#include "UserInterface.h"
#include "mp3player.h"
#include "Arduino.h"

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


void UserInterface::begin( QueueHandle_t *pPlayerCommandQueue )
{
    ESP_LOGD(TAG, "Start User Interface Task");

    m_pRfReader->PCD_Init();		            // Init MFRC522
	m_pRfReader->PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

    //save the queue where we must send our commands
    m_pPlayerQueue = pPlayerCommandQueue;

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

    //prepare some internal variables
    m_CardStatus = NoCard;

    //check if the command queue is set
    if (m_pPlayerQueue == NULL)
    {
        ESP_LOGE(TAG, "Player command queue was not defined.");
        return;
    }

    while (true)
    {

        switch (m_CardStatus)
        {
            //Check if the card is still present
            case ValidCard:
            case UnknownCard:
                {
                    bool result = false;

                    // Since wireless communication is voodoo we'll give it a few retrys before killing the music
                    for (uint32_t counter = 0; counter < 3; counter++) 
                    {
                        // Detect Tag without looking for collisions
                        byte bufferATQA[2];
                        byte bufferSize = sizeof(bufferATQA);

                        MFRC522::StatusCode status = m_pRfReader->PICC_WakeupA(bufferATQA, &bufferSize);

                        if (status == MFRC522::STATUS_OK)
                        {
                            if (m_pRfReader->PICC_ReadCardSerial() == true) 
                            {
                                bool uidEqual = true;

                                for (uint32_t count = 0; count < m_lastCardUidSize; count++)
                                {
                                    if (m_pRfReader->uid.uidByte[count] != m_lastCardUid[count])
                                    {
                                        uidEqual = false;
                                        break;
                                    }
                                }

                                if (uidEqual == true) 
                                {
                                    result = true;
                                    break;
                                }
                            }
                        }
                    }

                    //if the card was not found
                    if (result == false)
                    {
                        if (m_CardStatus == CardStatus::ValidCard)
                        {
                            ESP_LOGD(TAG,"Card disappeared, stopping playback.");

                            // stop playback
                            PlayerControlMessage_s myMessage = { .Command = CMD_STOP };

                            if (xQueueSend( *m_pPlayerQueue, &myMessage, ( TickType_t ) 0 ) )
                            {
                                ESP_LOGD(TAG, "send to queue successfull");
                            } else {
                                ESP_LOGW(TAG, "send to queue failed");
                            }
                        }
                        else 
                        {
                            ESP_LOGD(TAG,"Card disappeared");
                        }

                        m_CardStatus = NoCard;
                    } 
                    else
                    {
                        delay(1000);
                    }
                    break;
                }
            case NoCard:

                // Look for new cards
                if ( m_pRfReader->PICC_IsNewCardPresent() == true) 
                {

                    ESP_LOGI(TAG, "New card detected");
                    m_CardStatus = CardStatus::ValidCard;

                    // Select one of the cards
                    if ( m_pRfReader->PICC_ReadCardSerial()) 
                    {
                        // Show some details of the PICC (that is: the tag/card)
                        DumpByteArray("Card UID:", m_pRfReader->uid.uidByte, m_pRfReader->uid.size);

                        //remember the UID to check if the was removed
                        m_lastCardUidSize = m_pRfReader->uid.size;
                        memcpy(m_lastCardUid, m_pRfReader->uid.uidByte, m_lastCardUidSize );

                        if (this->ReadInformationFromTag() == true) 
                        {
                            ESP_LOGD(TAG, "Valid tag found");

                            //check volume

                            //check flags

                            //send filename to player
                            if (m_NfcTag.pTarget != NULL) 
                            {
                                PlayerControlMessage_s myMessage = { .Command = CMD_PLAY_FILE };
                                myMessage.pFileToPlay = m_NfcTag.pTarget;

                                if (xQueueSend( *m_pPlayerQueue, &myMessage, ( TickType_t ) 0 ) )
                                {
                                    ESP_LOGD(TAG, "send to queue successfull");
                                } else {
                                    ESP_LOGW(TAG, "send to queue failed");

                                    //if the send failed, we must do the job
                                    free(m_NfcTag.pTarget);
                                }

                                //if successful or not, our pointer to the file is no longer needed
                                m_NfcTag.pTarget = NULL;
                            }
                        } 
                        else 
                        {
                            ESP_LOGI(TAG, "No valid tag found / could no read information");
                            m_CardStatus = CardStatus::UnknownCard;
                        }

                        //end communication with the card
                        m_pRfReader->PICC_HaltA();
                        m_pRfReader->PCD_StopCrypto1();
                    }
                }

                //wait 250ms before we check again for a new card
                delay(250);

                break;

            default:
                m_CardStatus = UserInterface::NoCard;
                ESP_LOGW(TAG, "illegal card status");
                break;
        };
    };
}


bool UserInterface::ReadInformationFromTag() {
    bool                    result = false;
    MFRC522::StatusCode     status;
    MFRC522::PICC_Type      piccType;

    uint8_t     startBlockNumber;
    uint32_t    pagesNeeded;
    uint32_t    cardBlockSize;

    uint8_t     sizeInformation = sizeof(m_NfcTag.Information.Raw);
    uint8_t     buffer[18];
    uint8_t     bufferSize = sizeof(buffer);

    //prepare the default key
    for (byte i = 0; i < 6; i++)
    {
        m_MFRC522Key.keyByte[i] = 0xFF;
    }

    //mark the data set as invalid
    m_NfcTag.TagValid = false;

    //get the type for the card
    piccType = m_pRfReader->PICC_GetType(m_pRfReader->uid.sak);
    ESP_LOGD(TAG, "PICC type: %s", m_pRfReader->PICC_GetTypeName(piccType));

    if ((piccType != MFRC522::PICC_TYPE_MIFARE_MINI ) &&
        (piccType != MFRC522::PICC_TYPE_MIFARE_1K ) &&
        (piccType != MFRC522::PICC_TYPE_MIFARE_4K ) &&
        (piccType != MFRC522::PICC_TYPE_MIFARE_UL ) )
    {
        ESP_LOGW(TAG, "unsupported card type");
        goto FinishReadInformation;
    }

    if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
        (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
        (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
    {
        //initiate the variables for this card type
        startBlockNumber = INFORMATION_BLOCK_MIFARE_1K;

        cardBlockSize   = 16;

        // Authenticate using key A
        ESP_LOGV(TAG, "Authenticating MIFARE Mini/1k/4k using key A...");
        status = m_pRfReader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, startBlockNumber, &m_MFRC522Key, &(m_pRfReader->uid));
    } 
    else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL ) 
    {
        byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the NFCtag

        startBlockNumber = INFORMATION_BLOCK_MIFARE_ULTRA;
        
        cardBlockSize     = 4; 

        // Authenticate using key A
        ESP_LOGV(TAG, "Authenticating MIFARE UL using key A...");
        status = m_pRfReader->PCD_NTAG216_AUTH(m_MFRC522Key.keyByte, pACK);
    }

    //check the authentification result
    if (status != MFRC522::STATUS_OK) {
        ESP_LOGW(TAG, "PCD_Authenticate() failed: %s", m_pRfReader->GetStatusCodeName(status));
        goto FinishReadInformation;
    }

    //check if the union has the correct size (compare uint8_t array and structure)
    if (sizeof(m_NfcTag.Information.Entry) != sizeInformation)
    {
        ESP_LOGE(TAG, "Information structure size is not equal to the readout array! (%u instead of %u)", sizeof(m_NfcTag.Information.Entry), sizeInformation);
        goto FinishReadInformation;
    }

    //compare the input structure with the expected size
    if (sizeInformation != INFORMATION_BLOCK_SIZE) 
    {
        ESP_LOGE(TAG, "Information structure has the wrong size! (%u instead of %u)", sizeInformation, INFORMATION_BLOCK_SIZE);
        goto FinishReadInformation;
    }


    // read information blocks 
    {
    pagesNeeded             = INFORMATION_BLOCK_SIZE / cardBlockSize;
    uint8_t *pDataTarget    = &(m_NfcTag.Information.Raw[0]);

    ESP_LOGV(TAG, "Reading data from %u block(s) starting at %u", pagesNeeded, startBlockNumber);

    for (uint32_t block = startBlockNumber; block < (startBlockNumber + pagesNeeded); block++ )
    {
        status = m_pRfReader->MIFARE_Read(block, buffer, &bufferSize);
        if (status != MFRC522::STATUS_OK) 
        {
            ESP_LOGW(TAG, "MIFARE_Read() failed: %s", m_pRfReader->GetStatusCodeName(status));
            goto FinishReadInformation;
        }

        //reading was successfull, we could copy the data into our own structure
        memcpy(pDataTarget, buffer, cardBlockSize);
        pDataTarget += cardBlockSize;

    }
    }
    //     status = m_pRfReader->MIFARE_Read(block, buffer, &bufferSize);
    //     if (status != MFRC522::STATUS_OK) 
    //     {
    //         ESP_LOGW(TAG, "MIFARE_Read() failed: %s", m_pRfReader->GetStatusCodeName(status));
    //         goto FinishReadInformation;
    //     }

    //     //reading was successfull, we could copy the data into our own structure
    //     memcpy(m_NfcTag.Information.Raw16, buffer, sizeInformation);

    DumpByteArray("Data in block :", m_NfcTag.Information.Raw, sizeInformation);

    if (m_NfcTag.Information.Entry.Header.Cookie == magicKey) 
    {
        //check the header version
        if (m_NfcTag.Information.Entry.Header.Version == 1)
        {
            ESP_LOGD(TAG, "Handling Information Version 1");

            //read target from card

            //reserve some memory for the string and fill it with '0'
            m_NfcTag.pTarget = (char *) malloc((m_NfcTag.Information.Entry.MetaData.TargetLength + 1) * sizeof(char));
            memset(m_NfcTag.pTarget, 0 , (m_NfcTag.Information.Entry.MetaData.TargetLength + 1) * sizeof(char));

            if (m_NfcTag.pTarget == NULL)
            {
                ESP_LOGE(TAG, "Could not allocate memory for file-name.");
                goto FinishReadInformation;
            }

            uint8_t     activeKeyBlock = 0;
            uint8_t     newKeyBlock = TARGET_BLOCK_MIFARE_1K;

            uint16_t    position = 0;

            if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
                (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
                (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
            {
                startBlockNumber = TARGET_BLOCK_MIFARE_1K;
            } 
            else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL ) 
            {
                startBlockNumber = TARGET_BLOCK_MIFARE_ULTRA;
            }
            

            //check how many bytes we need
            if (m_NfcTag.Information.Entry.MetaData.TargetLength == 0)
            {
                ESP_LOGE(TAG, "String length of ZERO given!");
                goto FinishReadInformation;
            }
            else if (m_NfcTag.Information.Entry.MetaData.TargetLength < cardBlockSize) 
            {
                sizeInformation = m_NfcTag.Information.Entry.MetaData.TargetLength;
            } 
            else
            {
                sizeInformation = cardBlockSize;
            } 

            while (position < m_NfcTag.Information.Entry.MetaData.TargetLength)
            {
                // additional authentification is only neccessary on "classic" cards
                if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
                    (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
                    (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
                {
                 
                    newKeyBlock = (startBlockNumber / 4) * 4 + 3;

                    //authentificate the block
                    if (newKeyBlock != activeKeyBlock) 
                    {
                        ESP_LOGV(TAG, "Authenticating using key A...");
                        status = m_pRfReader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, newKeyBlock, &m_MFRC522Key, &(m_pRfReader->uid));
                        if (status != MFRC522::STATUS_OK) {
                            ESP_LOGW(TAG, "PCD_Authenticate() failed: %s", m_pRfReader->GetStatusCodeName(status));
                            goto FinishReadInformation;
                        }

                        activeKeyBlock = newKeyBlock;
                    }
                }

                // read block
                status = m_pRfReader->MIFARE_Read(startBlockNumber, buffer, &bufferSize);
                if (status != MFRC522::STATUS_OK) 
                {
                    ESP_LOGW(TAG, "MIFARE_Read() failed: %s", m_pRfReader->GetStatusCodeName(status));
                    goto FinishReadInformation;
                }

                //reading was successfull, we could copy the data into our own structure
                memcpy((m_NfcTag.pTarget + position), buffer, sizeInformation);

                //remember the number of byte read
                position += sizeInformation;

                startBlockNumber++;

                // additional authentification is only neccessary on "classic" cards
                if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
                    (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
                    (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
                {
                    // make sure the next block is not a key block
                    if ((startBlockNumber > 2) && (((startBlockNumber - 3) % 4) == 0 ))
                    {
                        startBlockNumber++;
                    }
                }
            } // read out loop
            
            ESP_LOGD(TAG, "Read target String: \"%s\"", m_NfcTag.pTarget);

            m_NfcTag.TagValid = true;
        }
        else 
        {
            ESP_LOGW(TAG, "Unknown Information Version");
            goto FinishReadInformation;
        }

        result = true;
    }


FinishReadInformation:
    if ((m_NfcTag.pTarget != NULL)&&(m_NfcTag.TagValid == false))
    {
        free(m_NfcTag.pTarget);
        m_NfcTag.pTarget = NULL;
    }

    return result;
}


bool UserInterface::WriteInformationToTag() {
    bool                    result = false;
    MFRC522::StatusCode     status;
    MFRC522::PICC_Type      piccType;

    //prepare the default key
    for (byte i = 0; i < 6; i++)
    {
        m_MFRC522Key.keyByte[i] = 0xFF;
    }

    // Show some details of the PICC (that is: the tag/card)
    DumpByteArray("Card UID:", m_pRfReader->uid.uidByte, m_pRfReader->uid.size);

    //check the type
    piccType = m_pRfReader->PICC_GetType(m_pRfReader->uid.sak);
    ESP_LOGD(TAG, "PICC type: %s", m_pRfReader->PICC_GetTypeName(piccType));

    if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
        (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
        (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
    {
        uint8_t     block = INFORMATION_BLOCK_MIFARE_1K;
        uint8_t     sizeInformation = sizeof(m_NfcTag.Information.Raw);

        // Authenticate using key A
        ESP_LOGV(TAG, "Authenticating using key A...");
        status = m_pRfReader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &m_MFRC522Key, &(m_pRfReader->uid));
        if (status != MFRC522::STATUS_OK) {
            ESP_LOGW(TAG, "PCD_Authenticate() failed: %s", m_pRfReader->GetStatusCodeName(status));
            goto FinishWriteInformation;
        }

        //check if the union has the correct size
        if ((sizeof(m_NfcTag.Information.Entry) != sizeInformation) || (sizeInformation > 16))
        {
            ESP_LOGE(TAG, "Information structure size is not equal to the readout array! (%u instead of %u)", sizeof(m_NfcTag.Information.Entry), sizeInformation);
            goto FinishWriteInformation;
        }

        ESP_LOGD(TAG, "writing information to block %u", block);
        status = m_pRfReader->MIFARE_Write(block, m_NfcTag.Information.Raw, sizeInformation);
        if (status != MFRC522::STATUS_OK) 
        {
            ESP_LOGW(TAG, "MIFARE_Write(%u) failed: %s", block, m_pRfReader->GetStatusCodeName(status));
            goto FinishWriteInformation;
        }
        ESP_LOGD(TAG, "writing information ok");

        //write the file name 
        if(m_NfcTag.Information.Entry.MetaData.TargetLength <= 16) 
        {
            block = TARGET_BLOCK_MIFARE_1K;

            ESP_LOGV(TAG, "Authenticating using key A...");
            status = m_pRfReader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &m_MFRC522Key, &(m_pRfReader->uid));
            if (status != MFRC522::STATUS_OK) {
                ESP_LOGW(TAG, "PCD_Authenticate() failed: %s", m_pRfReader->GetStatusCodeName(status));
                goto FinishWriteInformation;
            }

            byte buffer[18];

            memset(buffer, 0, 18);
            memcpy(buffer, m_NfcTag.pTarget, m_NfcTag.Information.Entry.MetaData.TargetLength);

            ESP_LOGD(TAG, "writing information to block %u", block);
            status = m_pRfReader->MIFARE_Write(block, buffer, 16);
            if (status != MFRC522::STATUS_OK) 
            {
                ESP_LOGW(TAG, "MIFARE_Write(%u) failed: %s", block, m_pRfReader->GetStatusCodeName(status));
                goto FinishWriteInformation;
            }
            ESP_LOGD(TAG, "writing information ok");
        }
    }

FinishWriteInformation:
    return result;
}

void UserInterface::DumpByteArray(const char *header, byte *buffer, byte bufferSize) 
{
    char        data[(bufferSize*3)+2];
    char        part[3];

    memset(data, 0, sizeof(data));

    for (byte i = 0; i < bufferSize; i++) 
    {
        memset(part, 0, 3);
        sprintf(part, "%02x", buffer[i]);

        strncat(data, part, 3);

        if(i < (bufferSize-1)) 
        {
            strncat(data, " ", 2);
        }
    }

    ESP_LOGD(TAG, "%s: %s", header, data);
}



void UserInterface::CleanUp( void )
{

}


                
                // } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL ) 
                // {
                //     byte PSWBuff[] = {0xFF, 0xFF, 0xFF, 0xFF}; //32 bit PassWord default FFFFFFFF
                //     byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the NFCtag

                //     block = 26;
                //     // Authenticate using key A
                //     Serial.println(F("Authenticating using key A..."));
                //     status = m_pRfReader->PCD_NTAG216_AUTH(&PSWBuff[0], pACK);
                //     if (status != MFRC522::STATUS_OK) {
                //         Serial.print(F("PCD_Authenticate() failed: "));
                //         Serial.println(m_pRfReader->GetStatusCodeName(status));
                //         //return;
                //     }

                //     Serial.print(F("Reading data from block ")); Serial.print(block);
                //     Serial.println(F(" ..."));
                //     status = m_pRfReader->MIFARE_Read(block, buffer, &size);
                //     if (status != MFRC522::STATUS_OK) {
                //         Serial.print(F("MIFARE_Read() failed: "));
                //         Serial.println(m_pRfReader->GetStatusCodeName(status));
                //     }
                //     Serial.print(F("Data in block ")); Serial.print(block); Serial.println(F(":"));
                //     dump_byte_array(buffer, 16); Serial.println();
                //     Serial.println();

                //     byte WBuff[] = {0x01, 0x02, 0x03, 0x04};
                //     status = m_pRfReader->MIFARE_Ultralight_Write(block, WBuff, 4);  //How to write to a page

                //     if (status != MFRC522::STATUS_OK) {
                //         Serial.print(F("MIFARE_Ultralight_Write() failed: "));
                //         Serial.println(m_pRfReader->GetStatusCodeName(status));
                //     }

                //     Serial.print(F("Reading data from block ")); Serial.print(block);
                //     Serial.println(F(" ..."));
                //     status = m_pRfReader->MIFARE_Read(block, buffer, &size);
                //     if (status != MFRC522::STATUS_OK) {
                //         Serial.print(F("MIFARE_Read() failed: "));
                //         Serial.println(m_pRfReader->GetStatusCodeName(status));
                //     }
                //     Serial.print(F("Data in block ")); Serial.print(block); Serial.println(F(":"));
                //     dump_byte_array(buffer, 16); Serial.println();
                //     Serial.println();


                // }