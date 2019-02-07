
#include "mp3player.h"

Mp3player::Mp3player(uint8_t _cs_pin = 25, uint8_t _dcs_pin = 26, uint8_t _dreq_pin = 32)
{
    m_pPlayer = new VS1053(_cs_pin, _dcs_pin, _dreq_pin);

    m_SystemFlagGroup   = NULL;
    m_volume            = 15;
}

Mp3player::~Mp3player()
{

}


void Mp3player::begin( QueueHandle_t *pCommandQueue )
{
    m_pPlayerQueue = pCommandQueue;

    if(m_SystemFlagGroup)
    {
        m_pPlayer->setSystemFlagGroup(m_SystemFlagGroup);
    }

    //create the task that will handle the playback
    xTaskCreate(
                    TaskFunctionAdapter,        /* Task function. */
                    "MP3 Player",       	    /* String with name of task. */
                    4* 1024,                    /* Stack size in words. */
                    this,                       /* Parameter passed as input of the task */
                    1,                          /* Priority of the task. */
                    &m_handle);                 /* Task handle. */
}

void Mp3player::SetSystemFlagGroup(EventGroupHandle_t eventGroup)
{
    m_SystemFlagGroup = eventGroup;
}


void Mp3player::TaskFunctionAdapter(void *pvParameters)
{
    Mp3player *mp3player = static_cast<Mp3player *>(pvParameters);

    mp3player->Run();

    mp3player->CleanUp();

    vTaskDelete(mp3player->m_handle);
}



void Mp3player::Run( void ) {
    PlayerControlMessage_s   PlayerControlMessage;

    m_pPlayer->begin();
    m_pPlayer->setVolume(m_volume);

    m_pPlayer->printVersion();
    //m_pPlayer->connecttoSD("/01.mp3"); // SD card

    //mp3.begin();
    //mp3.setVolume(volume);
    //mp3.connecttoSD("/01.mp3"); // SD card
    //mp3.connecttohost("edge.audio.3qsdn.com/senderkw-mp3");
    //mp3.connecttohost("https://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_fouteuur_96.mp3");

//TODO check if queue is set


    while (true)
    {
        m_pPlayer->loop();

        if( xQueueReceive( *m_pPlayerQueue, &(PlayerControlMessage), ( TickType_t ) 50 ) ) 
        {
            ESP_LOGV(TAG, "Received Command %u", PlayerControlMessage.Command);

            if ((PlayerControlMessage.Command == CMD_PLAY_FILE) || 
                (PlayerControlMessage.Command == CMD_RESUME_FILE))
            {
                //make sure the file exists
                if (PlayerControlMessage.pFileToPlay != NULL)
                {
                    //remove white characters
                    PlayerControlMessage.pFileToPlay->trim();

                    ESP_LOGD(TAG, "Received Path %s", PlayerControlMessage.pFileToPlay->c_str());
                    
                    if (PlayerControlMessage.pFileToPlay->charAt(0) == '/') 
                    {
                        String fileExtension = PlayerControlMessage.pFileToPlay->substring(PlayerControlMessage.pFileToPlay->lastIndexOf('.') + 1, PlayerControlMessage.pFileToPlay->length());
                        
                        fileExtension.toUpperCase();

                        ESP_LOGV(TAG, "Play File with Extension \"%s\"", fileExtension.c_str());

                        if (fileExtension.equals("MP3") || fileExtension.equals("M3U"))
                        {
                            m_pPlayer->connecttoSD(*(PlayerControlMessage.pFileToPlay), (PlayerControlMessage.Command == CMD_RESUME_FILE)?true:false);
                        }
                        else 
                        {
                            ESP_LOGW(TAG, "Unsupported File Extension");
                        }
                    } 
                    else if (PlayerControlMessage.pFileToPlay->startsWith("http"))
                    {
                        ESP_LOGV(TAG, "Play Stream");

                        m_pPlayer->connecttohost(*(PlayerControlMessage.pFileToPlay));
                    } 
                    else 
                    {
                        ESP_LOGW(TAG, "Unsupported File Type");
                    }

                    delete PlayerControlMessage.pFileToPlay;
                }
            }
            else if (PlayerControlMessage.Command == CMD_STOP) 
            {
                ESP_LOGD(TAG, "Received stop");
                m_pPlayer->stop_mp3client();
            }
            else if (PlayerControlMessage.Command == CMD_VOL_UP)
            {
                if (m_volume < 21)
                {
                    m_volume++;
                    m_pPlayer->setVolume(m_volume);
                }
            }
            else if (PlayerControlMessage.Command == CMD_VOL_DOWN)
            {
                if (m_volume > 0)
                {
                    m_volume--;
                    m_pPlayer->setVolume(m_volume);
                }
            }
        }

        delay(50);
    };
}


void Mp3player::CleanUp( void )
{

}

QueueHandle_t *Mp3player::getQueue( void )
{
    return m_pPlayerQueue;
}