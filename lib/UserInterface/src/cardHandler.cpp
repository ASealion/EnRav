#include "cardHandler.h"
#include "pinout.h"

CardHandler::CardHandler() 
{    
    #if (defined MFRC522_CS) && (defined MFRC522_RST)
        m_pRfReader = new MFRC522(MFRC522_CS, MFRC522_RST);    
    #else
        m_pRfReader = NULL;
    #endif
}

CardHandler::CardHandler(MFRC522 *pCardReader) 
{        
    m_pRfReader = pCardReader;
}

CardHandler::~CardHandler()
{    
}


void CardHandler::connectCardReader(MFRC522 *pCardReader)
{
    String myVersion;

    ESP_LOGD(TAG, "Connecting RFID reader...");    

    m_pRfReader = pCardReader;

    m_pRfReader->PCD_Init();                // Init MFRC522

	// Get the MFRC522 firmware version
	byte v = m_pRfReader->PCD_ReadRegister(MFRC522::VersionReg);

	// Lookup which version
	switch(v) {
		case 0x88: myVersion = String("(clone)");            break;
		case 0x90: myVersion = String("v0.0");               break;
		case 0x91: myVersion = String("v1.0");               break;
		case 0x92: myVersion = String("v2.0");               break;
		case 0x12: myVersion = String("counterfeit chip");   break;
		default:   myVersion = String("(unknown)");
	}

    ESP_LOGI(TAG, "Firmware Version: 0x%02x = %s", v, myVersion.c_str());
	

	// When 0x00 or 0xFF is returned, communication probably failed
	if ((v == 0x00) || (v == 0xFF))
    {
	    ESP_LOGW(TAG, "WARNING: Communication failure, is the MFRC522 properly connected?");
    }
}


bool CardHandler::IsNewCardPresent( void )
{
    bool result = false;

    //make sure we are attached to a RFID reader
    if (m_pRfReader)
    {

        // Look for new card
        if ( m_pRfReader->PICC_IsNewCardPresent() == true) 
        {
            result = true;
        }
    } 

    //some simple debug test data    
    else 
    {
        result = true;
    }

    return result;    
}


bool CardHandler::IsCardPresent( CardSerialNumber *pActualCardSerial )
{
    bool result = false;

    //make sure we are attached to a RFID reader
    if (m_pRfReader)
    {
        // Since wireless communication is voodoo we'll give it a few retrys before killing the music
        for (uint32_t counter = 0; counter < 3; counter++) 
        {
            // Detect Tag without looking for collisions
            byte bufferATQA[2];
            byte bufferSize = sizeof(bufferATQA);

            MFRC522::StatusCode status = m_pRfReader->PICC_WakeupA(bufferATQA, &bufferSize);

            if (status == MFRC522::STATUS_OK)
            {
                if (m_pRfReader->PICC_ReadCardSerial()) 
                {
                    bool uidEqual = true;

                    //compare if the Uids have the same size
                    if (pActualCardSerial->SerialNumberLength == m_pRfReader->uid.size)
                    {
                        //check the diffferent bytes
                        for (uint32_t counter=0; counter < pActualCardSerial->SerialNumberLength; counter++)
                        {
                            if (m_pRfReader->uid.uidByte[counter] != pActualCardSerial->SerialNumber[counter])
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
        } // "magic loop"
    }

    //some simple debug test data    
    else 
    {
        result = true;
    }

    return result;    
}


bool CardHandler::GetCardSerial(CardSerialNumber *pActualCardSerial)
{
    bool result = false;
    
    //make sure we are attached to a RFID reader
    if (m_pRfReader)
    {
        // Read the serial of one card
        if ( m_pRfReader->PICC_ReadCardSerial()) 
        {
            memset(pActualCardSerial->SerialNumber, 0, sizeof(pActualCardSerial->SerialNumber));

            pActualCardSerial->SerialNumberLength = m_pRfReader->uid.size;
            memcpy(pActualCardSerial->SerialNumber, m_pRfReader->uid.uidByte, pActualCardSerial->SerialNumberLength );

            result = true;
        }
    }

    //some simple debug test data
    else 
    {        
        memset(pActualCardSerial->SerialNumber, 0, sizeof(pActualCardSerial->SerialNumber));

        pActualCardSerial->SerialNumberLength = 4;
        pActualCardSerial->SerialNumber[0] = 15;
        pActualCardSerial->SerialNumber[1] = 16;
        pActualCardSerial->SerialNumber[2] = 17;
        pActualCardSerial->SerialNumber[3] = 18;

        result = true;
    }

    return result;
}


bool CardHandler::ReadCardInformation(CardData *pTarget)
{
    bool result = false;

    //we start with am invalid result
    pTarget->m_valid = false;

    if (m_pRfReader != NULL)
    {
        MFRC522::StatusCode     status;
        MFRC522::PICC_Type      piccType;

        CardDataBlock_s         cardDataBlock;

        uint8_t                 startBlockNumber;
        uint32_t                cardBlockSize;          //the number of bytes we could read from every block

        uint32_t                pagesNeeded;
        uint8_t                 *pDataTarget;

        uint8_t                 buffer[18];
        uint8_t                 bufferSize = sizeof(buffer);

        char                    *pFileName = NULL;

        //check if the union has the correct size (compare uint8_t array and structure)
        if (sizeof(cardDataBlock.Entry) != sizeof(cardDataBlock.Raw))
        {
            ESP_LOGE(TAG, "Information structure size is not equal to the readout array! (%u instead of %u)", sizeof(cardDataBlock.Entry), sizeof(cardDataBlock.Raw));
            goto FinishReadInformation;
        }

        //compare the input structure with the expected size
        if (sizeof(cardDataBlock.Raw) != INFORMATION_BLOCK_SIZE) 
        {
            ESP_LOGE(TAG, "Information structure has the wrong size! (%u instead of %u)", sizeof(cardDataBlock.Raw), INFORMATION_BLOCK_SIZE);
            goto FinishReadInformation;
        }

        //prepare the default key
        for (uint32_t counter=0; counter < sizeof(m_MFRC522Key.keyByte); counter++)
        {
            m_MFRC522Key.keyByte[counter] = 0xFF;
        }

        //get the type for the card
        piccType = m_pRfReader->PICC_GetType(m_pRfReader->uid.sak);
        ESP_LOGD(TAG, "PICC type: %s", m_pRfReader->PICC_GetTypeName(piccType));

        if ((piccType != MFRC522::PICC_TYPE_MIFARE_MINI ) &&
            (piccType != MFRC522::PICC_TYPE_MIFARE_1K ) &&
            (piccType != MFRC522::PICC_TYPE_MIFARE_4K ) &&
            (piccType != MFRC522::PICC_TYPE_MIFARE_UL ) )
        {
            ESP_LOGW(TAG, "Unsupported card type deteced");
            goto FinishReadInformation;
        }

        //authentificate with the card
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
            ESP_LOGW(TAG, "Card Authentification failed: %s", m_pRfReader->GetStatusCodeName(status));
            goto FinishReadInformation;
        }

        // read information block(s)
        pagesNeeded  = INFORMATION_BLOCK_SIZE / cardBlockSize;
        pDataTarget  = &(cardDataBlock.Raw[0]);

        ESP_LOGV(TAG, "Reading data from %u block(s) starting at block %u", pagesNeeded, startBlockNumber);

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

        //DumpByteArray("Data from card: ", cardDataBlock.Raw, sizeof(cardDataBlock.Raw));

        if (cardDataBlock.Entry.Header.Cookie != INFORMATION_BLOCK__MAGIC_KEY) 
        {
            ESP_LOGI(TAG, "Wrong Magic Key (%08x), card is not for this box", cardDataBlock.Entry.Header.Cookie);
            goto FinishReadInformation;
        }


        //check the header version
        ESP_LOGI(TAG, "Card Version %u found:", cardDataBlock.Entry.Header.Version );

        if (cardDataBlock.Entry.Header.Version == 1)
        {
            uint8_t     activeKeyBlock = 0;
            uint8_t     newKeyBlock = TARGET_BLOCK_MIFARE_1K;

            uint16_t    position = 0;


            // handle configuration


            // handle last positions
            pTarget->m_PlaylistPosition = cardDataBlock.Entry.MetaData.LastListPostion;
            pTarget->m_TrackPosition    = cardDataBlock.Entry.MetaData.LastFilePosition;

            //get the file string
            //reserve some memory for the string and fill it with '0'
            pFileName = (char *) malloc((cardDataBlock.Entry.MetaData.FileNameLength + 1) * sizeof(char));
            memset(pFileName, 0 , (cardDataBlock.Entry.MetaData.FileNameLength + 1) * sizeof(char));

            if (pFileName == NULL)
            {
                ESP_LOGE(TAG, "Could not allocate memory for file-name.");
                goto FinishReadInformation;
            }

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
            if (cardDataBlock.Entry.MetaData.FileNameLength == 0)
            {
                ESP_LOGE(TAG, "String length of ZERO given!");
                goto FinishReadInformation;
            }
            else if (cardDataBlock.Entry.MetaData.FileNameLength < cardBlockSize) 
            {
                cardBlockSize = cardDataBlock.Entry.MetaData.FileNameLength;
            } 

            while (position < cardDataBlock.Entry.MetaData.FileNameLength)
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
                            ESP_LOGW(TAG, "Authentication failed for block %u: %s", newKeyBlock, m_pRfReader->GetStatusCodeName(status));
                            goto FinishReadInformation;
                        }

                        activeKeyBlock = newKeyBlock;
                    }
                }

                // read block
                status = m_pRfReader->MIFARE_Read(startBlockNumber, buffer, &bufferSize);
                if (status != MFRC522::STATUS_OK) 
                {
                    ESP_LOGW(TAG, "MIFARE_Read() failed on block %u: %s", startBlockNumber, m_pRfReader->GetStatusCodeName(status));
                    goto FinishReadInformation;
                }

                //reading was successfull, we could copy the data into our own structure
                memcpy((pFileName + position), buffer, cardBlockSize);

                //remember the number of byte read
                position += cardBlockSize;

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
            
            // coyp the string into the result structure
            pTarget->m_fileName = String(pFileName);

            ESP_LOGV(TAG, "Read target String: \"%s\"", pTarget->m_fileName.c_str());

            pTarget->m_valid = true;
        }
        else 
        {
            ESP_LOGW(TAG, "Unknown information version");
            result = false;
            goto FinishReadInformation;
        }

        // finish the read
        result = true;

        FinishReadInformation:

            //make sure we clear the file name buffer
            if (pFileName != NULL)
            {
                free(pFileName);
                pFileName = NULL;
            }
    }

    //create some dummy data for testing purpose
    else
    {
        pTarget->m_fileName = "/00.mp3";

        result = true;
    }

    return result;
}

bool CardHandler::WriteCardInformation(CardData *pSource) 
{
    bool result = false;

    if (m_pRfReader != NULL)
    {
        CardDataBlock_s         cardDataBlock;

        MFRC522::StatusCode     status;
        MFRC522::PICC_Type      piccType;

        uint8_t                 startBlockNumber;
        uint32_t                cardBlockSize;          //the number of bytes we could read from every block


        //check if the union has the correct size (compare uint8_t array and structure)
        if (sizeof(cardDataBlock.Entry) != sizeof(cardDataBlock.Raw))
        {
            ESP_LOGE(TAG, "Information structure size is not equal to the readout array! (%u instead of %u)", sizeof(cardDataBlock.Entry), sizeof(cardDataBlock.Raw));
            goto FinishWriteInformation;
        }

        //compare the input structure with the expected size
        if (sizeof(cardDataBlock.Raw) != INFORMATION_BLOCK_SIZE) 
        {
            ESP_LOGE(TAG, "Information structure has the wrong size! (%u instead of %u)", sizeof(cardDataBlock.Raw), INFORMATION_BLOCK_SIZE);
            goto FinishWriteInformation;
        }

        //prepare the default key
        for (uint32_t counter=0; counter < sizeof(m_MFRC522Key.keyByte); counter++)
        {
            m_MFRC522Key.keyByte[counter] = 0xFF;
        }

        //prepare the internal strucure
        cardDataBlock.Entry.Header.Cookie               = INFORMATION_BLOCK__MAGIC_KEY;
        cardDataBlock.Entry.Header.Version              = 1;

        cardDataBlock.Entry.MetaData.Configuration      = 0;        
        cardDataBlock.Entry.MetaData.LastListPostion    = 0;
        cardDataBlock.Entry.MetaData.LastFilePosition   = 0;
        cardDataBlock.Entry.MetaData.FileNameLength     = pSource->m_fileName.length();

        //limit file name length to 255 characters
        if (cardDataBlock.Entry.MetaData.FileNameLength > 255)
        {
            ESP_LOGE(TAG, "Maximum supported File Name legth is 255 (%u)", cardDataBlock.Entry.MetaData.FileNameLength);
            goto FinishWriteInformation;
        }

        //get the type for the card
        piccType = m_pRfReader->PICC_GetType(m_pRfReader->uid.sak);
        ESP_LOGD(TAG, "PICC type: %s", m_pRfReader->PICC_GetTypeName(piccType));

        if ((piccType != MFRC522::PICC_TYPE_MIFARE_MINI ) &&
            (piccType != MFRC522::PICC_TYPE_MIFARE_1K ) &&
            (piccType != MFRC522::PICC_TYPE_MIFARE_4K ) /*&&
            (piccType != MFRC522::PICC_TYPE_MIFARE_UL )*/ )
        {
            ESP_LOGW(TAG, "Unsupported card type deteced");
            goto FinishWriteInformation;
        }

        //authentificate with the card
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
        // else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL ) 
        // {
        //     byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the NFCtag

        //     startBlockNumber = INFORMATION_BLOCK_MIFARE_ULTRA;
            
        //     cardBlockSize     = 4; 

        //     // Authenticate using key A
        //     ESP_LOGV(TAG, "Authenticating MIFARE UL using key A...");
        //     status = m_pRfReader->PCD_NTAG216_AUTH(m_MFRC522Key.keyByte, pACK);
        // }

        ESP_LOGV(TAG, "Writing information to block %u", block);
        status = m_pRfReader->MIFARE_Write(startBlockNumber, cardDataBlock.Raw, cardBlockSize);


        if (status != MFRC522::STATUS_OK)
        {
            ESP_LOGW(TAG, "MIFARE_Write(%u) failed: %s", startBlockNumber, m_pRfReader->GetStatusCodeName(status));
            goto FinishWriteInformation;
        }

        ESP_LOGD(TAG, "Wrote information block to card");

        //write the file name 
        if(cardDataBlock.Entry.MetaData.FileNameLength <= 16) 
        {
            startBlockNumber = TARGET_BLOCK_MIFARE_1K;

            ESP_LOGV(TAG, "Authenticating using key A...");
            status = m_pRfReader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, startBlockNumber, &m_MFRC522Key, &(m_pRfReader->uid));
            if (status != MFRC522::STATUS_OK) {
                ESP_LOGW(TAG, "PCD_Authenticate() failed: %s", m_pRfReader->GetStatusCodeName(status));
                goto FinishWriteInformation;
            }

            byte buffer[18];

            memset(buffer, 0, 18);
            memcpy(buffer, pSource->m_fileName.c_str(), cardDataBlock.Entry.MetaData.FileNameLength);

            ESP_LOGD(TAG, "writing information to block %u", startBlockNumber);

            status = m_pRfReader->MIFARE_Write(startBlockNumber, buffer, 16);
            if (status != MFRC522::STATUS_OK) 
            {
                ESP_LOGW(TAG, "MIFARE_Write(%u) failed: %s", startBlockNumber, m_pRfReader->GetStatusCodeName(status));
                goto FinishWriteInformation;
            }

            ESP_LOGI(TAG, "writing information ok");

            result = true;
        }
    }

    FinishWriteInformation:
    
    return result;
 }


//                 // } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL ) 
//                 // {
//                 //     byte PSWBuff[] = {0xFF, 0xFF, 0xFF, 0xFF}; //32 bit PassWord default FFFFFFFF
//                 //     byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the NFCtag

//                 //     block = 26;
//                 //     // Authenticate using key A
//                 //     Serial.println(F("Authenticating using key A..."));
//                 //     status = m_pRfReader->PCD_NTAG216_AUTH(&PSWBuff[0], pACK);
//                 //     if (status != MFRC522::STATUS_OK) {
//                 //         Serial.print(F("PCD_Authenticate() failed: "));
//                 //         Serial.println(m_pRfReader->GetStatusCodeName(status));
//                 //         //return;
//                 //     }

//                 //     Serial.print(F("Reading data from block ")); Serial.print(block);
//                 //     Serial.println(F(" ..."));
//                 //     status = m_pRfReader->MIFARE_Read(block, buffer, &size);
//                 //     if (status != MFRC522::STATUS_OK) {
//                 //         Serial.print(F("MIFARE_Read() failed: "));
//                 //         Serial.println(m_pRfReader->GetStatusCodeName(status));
//                 //     }
//                 //     Serial.print(F("Data in block ")); Serial.print(block); Serial.println(F(":"));
//                 //     dump_byte_array(buffer, 16); Serial.println();
//                 //     Serial.println();

//                 //     byte WBuff[] = {0x01, 0x02, 0x03, 0x04};
//                 //     status = m_pRfReader->MIFARE_Ultralight_Write(block, WBuff, 4);  //How to write to a page

//                 //     if (status != MFRC522::STATUS_OK) {
//                 //         Serial.print(F("MIFARE_Ultralight_Write() failed: "));
//                 //         Serial.println(m_pRfReader->GetStatusCodeName(status));
//                 //     }

//                 //     Serial.print(F("Reading data from block ")); Serial.print(block);
//                 //     Serial.println(F(" ..."));
//                 //     status = m_pRfReader->MIFARE_Read(block, buffer, &size);
//                 //     if (status != MFRC522::STATUS_OK) {
//                 //         Serial.print(F("MIFARE_Read() failed: "));
//                 //         Serial.println(m_pRfReader->GetStatusCodeName(status));
//                 //     }
//                 //     Serial.print(F("Data in block ")); Serial.print(block); Serial.println(F(":"));
//                 //     dump_byte_array(buffer, 16); Serial.println();
//                 //     Serial.println();


//                 // }


void CardHandler::StopCommunication(void)
{
    //end communication with the card
    m_pRfReader->PICC_HaltA();
    m_pRfReader->PCD_StopCrypto1();
}

