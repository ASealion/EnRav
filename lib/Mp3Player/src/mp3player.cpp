
#include "mp3player.h"

Mp3player::Mp3player(uint8_t _cs_pin = 25, uint8_t _dcs_pin = 26, uint8_t _dreq_pin = 32)
{
    m_pPlayer = new VS1053(_cs_pin, _dcs_pin, _dreq_pin);
}

Mp3player::~Mp3player()
{

}


void Mp3player::begin( QueueHandle_t *pCommandQueue )
{
    m_pPlayerQueue = pCommandQueue;

    //create the task that will handle the playback
    xTaskCreate(
                    TaskFunctionAdapter,        /* Task function. */
                    "MP3 Player",       	    /* String with name of task. */
                    4* 1024,                    /* Stack size in words. */
                    this,                       /* Parameter passed as input of the task */
                    1,                          /* Priority of the task. */
                    &m_handle);                 /* Task handle. */
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
    m_pPlayer->setVolume(15);
    m_pPlayer->connecttoSD("/01.mp3"); // SD card

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

            if (PlayerControlMessage.Command == CMD_PLAY_FILE) 
            {
                //make sure the file exists
                if (PlayerControlMessage.pFileToPlay != NULL)
                {
                    ESP_LOGD(TAG, "Received Path %s", PlayerControlMessage.pFileToPlay);

                    m_pPlayer->connecttoSD(PlayerControlMessage.pFileToPlay); 

                    free(PlayerControlMessage.pFileToPlay);

                }
            }
            else if (PlayerControlMessage.Command == CMD_STOP) 
            {
                ESP_LOGD(TAG, "Received stop");
                m_pPlayer->stop_mp3client();
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