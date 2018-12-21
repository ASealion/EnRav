#include "UserInterface.h"
#include "pinout.h"

#ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
#else
    static const char *TAG = "UserInterface";
#endif


UserInterface::UserInterface() : m_CardHandler()
{
    m_handle                = NULL;
    m_CardStatus            = RfidCardStatus::NoCard;
    m_InterfaceCommandQueue = xQueueCreate( 5, sizeof( InterfaceCommandMessage_s ) );

    // these pointer must be set from "extern"
    m_pPlayerQueue          = NULL;    
    
}

UserInterface::~UserInterface()
{
}


QueueHandle_t *UserInterface::getInterfaceCommandQueue(void) 
{
    return &m_InterfaceCommandQueue;
}


void UserInterface::setPlayerCommandQueue(QueueHandle_t *pCommandQueue)
{
    m_pPlayerQueue = pCommandQueue;
}


void UserInterface::begin( void )
{
    ESP_LOGD(TAG, "Start User Interface Task");

    //check the settings
    if (m_pPlayerQueue == NULL)
    {
        ESP_LOGE(TAG, "Could not start without MP3 Player Queue");
    }

    m_CardHandler.connectCardReader();

    //create the task that will handle all user interactions
    xTaskCreate(
                    TaskFunctionAdapter,        /* Task function. */
                    "UserInterface",       	    /* String with name of task. */
                    4* 1024,                    /* Stack size in words. */
                    this,                       /* Parameter passed as input of the task */
                    1,                          /* Priority of the task. */
                    &m_handle);                 /* Task handle. */

}

//we need this function to get called by the "C" RTOS
void UserInterface::TaskFunctionAdapter(void *pvParameters)
{
    UserInterface *userInterface = static_cast<UserInterface *>(pvParameters);

    userInterface->run();

    userInterface->cleanUp();

    vTaskDelete(userInterface->m_handle);
}



void UserInterface::run( void ) 
{
    InterfaceCommandMessage_s       InterfaceCommandMessage;
    ESP_LOGD(TAG, "User Interface Thread started");

    while (true)
    {
        //set/clear LEDs


        //check buttons


        //check GyroSensor


        //handle RFID-Cards        
        switch (m_CardStatus)
        {
            case RfidCardStatus::UnknownCard:
            case RfidCardStatus::ValidCard:

                //do this check only if at least 1000ms have elapsed since the last check
                if (TimeElapsed(m_CardTimestamp) >= 1000)
                {
                    
                    //remember the new time
                    m_CardTimestamp = millis();

                    //check if the card was removed
                    if (m_CardHandler.IsCardPresent(&m_CardSerialNumber) == false)
                    {
                        //if it was a valid card, we must stop the playback
                        if (m_CardStatus == RfidCardStatus::ValidCard)
                        {
                            ESP_LOGI(TAG,"EnRav Card removed, stopping playback.");

                            //make sure we have a player queue available
                            if (m_pPlayerQueue != NULL)
                            {
                                // stop playback
                                Mp3player::PlayerControlMessage_s newMessage = { .Command = Mp3player::CMD_STOP };

                                // the message is copied to the queue, so no need for the original one :)
                                if (xQueueSend( *m_pPlayerQueue, &newMessage, ( TickType_t ) 0 ) )
                                {
                                    ESP_LOGD(TAG, "Send Stop Command to queue");
                                } else {
                                    ESP_LOGE(TAG, "Send to queue failed");
                                }
                            }
                            else
                            {
                                ESP_LOGW(TAG, "No Player Queue");
                            }
                        }
                        // if it is an unkown card we must do nothing
                        else 
                        {
                            ESP_LOGI(TAG,"Card removed from reader");
                        }

                        m_CardStatus = RfidCardStatus::NoCard;

                    }
                }

                break;
            
            //there is no known card on the reader
            case RfidCardStatus::NoCard:

                //do this check only if at least 250ms have elapsed
                if (TimeElapsed(m_CardTimestamp) >= 250)
                {
                    
                    //remember the new time
                    m_CardTimestamp = millis();

                    //check if a card is put onto the reader
                    if ( m_CardHandler.IsNewCardPresent() == true) 
                    {

                        ESP_LOGD(TAG, "New card detected");

                        m_CardStatus = RfidCardStatus::UnknownCard;

                        // Try to get the serial of the card
                        if ( m_CardHandler.GetCardSerial(&m_CardSerialNumber)) 
                        {

                            ESP_LOGI(TAG, "Card Serial is \"%s\"", m_CardSerialNumber.toString().c_str());

                            //try to read the information from the card
                            if ((m_CardHandler.ReadCardInformation(&m_CardData)) && (m_CardData.GetValid()))
                            {
                                ESP_LOGI(TAG, "Valid EnRav tag found");
                                m_CardStatus = RfidCardStatus::ValidCard;

                                //check options

                                //play file (is set)
                                if (m_CardData.m_fileName.length()) 
                                {

                                    if (m_pPlayerQueue != NULL) 
                                    {
                                        Mp3player::PlayerControlMessage_s newMessage = { .Command = Mp3player::CMD_PLAY_FILE };

                                        //create a new String Object and attach the pointer to the new message
                                        newMessage.pFileToPlay = new String(m_CardData.m_fileName);

                                        if (xQueueSend( *m_pPlayerQueue, &newMessage, ( TickType_t ) 0 ) )
                                        {
                                            ESP_LOGD(TAG, "Send \"Play File Message\" to queue");
                                        } else {
                                            ESP_LOGE(TAG, "Send to player queue failed");

                                            // //if the send failed, we must do the job of deleting the string
                                            delete(newMessage.pFileToPlay);
                                        }
                                    }

                                    ESP_LOGD(TAG, "Requesting file \"%s\"", m_CardData.m_fileName.c_str() );
                                }

                                //check playlist position


                                //check file position

                            } 
                            else 
                            {
                                ESP_LOGI(TAG, "No valid tag found / could no read information");
                                m_CardStatus = RfidCardStatus::UnknownCard;
                            }


                            //end communication with the card
                            m_CardHandler.StopCommunication();
                        }
                    } // new card found
                } // time elapsed

               break;
            default:
                m_CardStatus = RfidCardStatus::NoCard;
                ESP_LOGE(TAG, "illegal rfid card status");
                break;
        }

        //check for "external" commands
        if( xQueueReceive( m_InterfaceCommandQueue, &(InterfaceCommandMessage), ( TickType_t ) 0 ) ) 
        {
            ESP_LOGD(TAG, "Received Command %u", InterfaceCommandMessage.Command);
            
            // CMD_SET_VOLUME,
            if (InterfaceCommandMessage.Command == UserInterface::CMD_SET_VOLUME) 
            {
                //make sure there is a volume
                if (InterfaceCommandMessage.pData != NULL)
                {
                    // String *pFileName = (String *) InterfaceCommandMessage.pData;

                    // ESP_LOGV(TAG, "Received FileName %s", pFileName->c_str());

                    // //TODO create "next" message

                    // delete (String*) InterfaceCommandMessage.pData;
                }
            }
            // CMD_VOLUME_UP, 
            else if (InterfaceCommandMessage.Command == UserInterface::CMD_VOLUME_UP) 
            {
                //make sure the file exists
                if (InterfaceCommandMessage.pData != NULL)
                {
                    // String *pFileName = (String *) InterfaceCommandMessage.pData;

                    // ESP_LOGV(TAG, "Received FileName %s", pFileName->c_str());

                    // //TODO create "next" message

                    // delete (String*) InterfaceCommandMessage.pData;
                }
            }
            // CMD_VOLUME_DOWN,
            else if (InterfaceCommandMessage.Command == UserInterface::CMD_VOLUME_DOWN) 
            {
                //make sure the file exists
                if (InterfaceCommandMessage.pData != NULL)
                {
                    // String *pFileName = (String *) InterfaceCommandMessage.pData;

                    // ESP_LOGV(TAG, "Received FileName %s", pFileName->c_str());

                    // //TODO create "next" message

                    // delete (String*) InterfaceCommandMessage.pData;
                }
            }               
            // CMD_CARD_WRITE,
            if (InterfaceCommandMessage.Command == UserInterface::CMD_CARD_WRITE) 
            {
                //make sure the file exists
                if (InterfaceCommandMessage.pData != NULL)
                {
                    CardData *pNewCard = (CardData *) InterfaceCommandMessage.pData;

                    //some parts of the structure should be zero if the card is new
                    pNewCard->m_PlaylistPosition    = 0;
                    pNewCard->m_TrackPosition       = 0;

                    ESP_LOGD(TAG, "Writing Card for %s", pNewCard->m_fileName.c_str());

                    m_CardHandler.WriteCardInformation(pNewCard);

                    delete (CardData *) InterfaceCommandMessage.pData;
                }
            }
            // CMD_PLAY_FILE,                
            else if (InterfaceCommandMessage.Command == UserInterface::CMD_PLAY_FILE) 
            {
                //make sure the file exists
                if (InterfaceCommandMessage.pData != NULL)
                {
                    String *pFileName = (String *) InterfaceCommandMessage.pData;

                    if ((pFileName->length()) && (m_pPlayerQueue != NULL)) 
                    {
                        Mp3player::PlayerControlMessage_s newMessage = { .Command = Mp3player::CMD_PLAY_FILE };

                        //attach the pointer to the next message
                        newMessage.pFileToPlay = pFileName;

                        if (xQueueSend( *m_pPlayerQueue, &newMessage, ( TickType_t ) 0 ) )
                        {
                            ESP_LOGD(TAG, "Send \"Play File Message\" for \"%s\" to queue", newMessage.pFileToPlay->c_str());
                        } 
                        else 
                        {
                            ESP_LOGE(TAG, "Send to player queue failed");

                            // //if the send failed, we must do the job of deleting the string
                            delete newMessage.pFileToPlay;
                        }
                    }
                    else 
                    {
                        delete (String*) InterfaceCommandMessage.pData;
                    }
                }
            }
            // CMD_PLAY_STOP,
            else if (InterfaceCommandMessage.Command == UserInterface::CMD_PLAY_STOP) 
            {
                ESP_LOGV(TAG, "Received stop");

                //make sure we have a player queue available
                if (m_pPlayerQueue != NULL)
                {
                    // stop playback
                    Mp3player::PlayerControlMessage_s newMessage = { .Command = Mp3player::CMD_STOP };

                    // the message is copied to the queue, so no need for the original one :)
                    if (xQueueSend( *m_pPlayerQueue, &newMessage, ( TickType_t ) 0 ) )
                    {
                        ESP_LOGD(TAG, "Send Stop Command to queue");
                    } else {
                        ESP_LOGE(TAG, "Send to queue failed");
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "No Player Queue");
                }                                
            } else {
                ESP_LOGW(TAG, "Unknown command reveived!");
            }
        }

        delay(5);        
    }
}

void UserInterface::cleanUp( void )
{

}     


uint32_t UserInterface::TimeElapsed(uint32_t TimeStamp)
{
    uint32_t result;
    uint32_t actualTimeStamp = millis();

	if (TimeStamp > actualTimeStamp) {
		result = 0xFFFFFFFF - TimeStamp + actualTimeStamp;
	} else {
		result = actualTimeStamp - TimeStamp;
	}

	return result;
}
