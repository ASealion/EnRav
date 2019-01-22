#include "LedHandler.h"

#include "pinout.h"
#include "SystemEventFlags.h"

LedHandler::LedHandler( )
{
    m_EventGroup = NULL;

    //prepare the pwm channels
    ledcSetup(m_channel_Red_1, m_frequency, m_resolution);
    ledcAttachPin(LED_RED_1, m_channel_Red_1);
    ledcSetup(m_channel_Red_2, m_frequency, m_resolution);
    ledcAttachPin(LED_RED_2, m_channel_Red_2);

    ledcSetup(m_channel_Green_1, m_frequency, m_resolution);
    ledcAttachPin(LED_GREEN_1, m_channel_Green_1);
    ledcSetup(m_channel_Green_2, m_frequency, m_resolution);
    ledcAttachPin(LED_GREEN_2, m_channel_Green_2);
}

LedHandler::~LedHandler()
{

}


bool LedHandler::begin( void )
{
    bool result = false;

    ESP_LOGD(TAG, "Start LED Task");

    //check the internal links
    if (m_EventGroup != NULL)
    {
        
        //create the task that will handle the playback
        xTaskCreate(
                        TaskFunctionAdapter,        /* Task function. */
                        "LED Handler",       	    /* String with name of task. */
                        4 * 512,                    /* Stack size in words. */
                        this,                       /* Parameter passed as input of the task */
                        5 ,                         /* Priority of the task. */
                        &m_handle);                 /* Task handle. */

        result = true;
    } 
    else 
    {
        ESP_LOGE(TAG, "Could not start task without system flags group");
    }

    return result;
}


bool LedHandler::SetEventGroup(EventGroupHandle_t  eventGroup)
{
    bool result = true;

    m_EventGroup = eventGroup;

    return result;
}



void LedHandler::TaskFunctionAdapter(void *pvParameters)
{
    LedHandler *ledHandler = static_cast<LedHandler *>(pvParameters);

    ledHandler->Run();

    ledHandler->CleanUp();

    vTaskDelete(ledHandler->m_handle);
}

void LedHandler::Run( void ) {
    EventBits_t eventBits;

    while (true)
    {

        eventBits = xEventGroupGetBits(m_EventGroup);
        //eventBits = xEventGroupGetBits(SystemFlagGroup);

        if (eventBits & SF_PLAYING_FILE) 
        {
            ledcWrite(m_channel_Red_1, 0);
            ledcWrite(m_channel_Red_2, 0);

            ledcWrite(m_channel_Green_1, 255);
            ledcWrite(m_channel_Green_2, 255);
        } 
        else
        {
            ledcWrite(m_channel_Red_1, 32);
            ledcWrite(m_channel_Red_2, 32);

            ledcWrite(m_channel_Green_1, 0);
            ledcWrite(m_channel_Green_2, 0);
        }
        // for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++) {
        //     ledcWrite(m_channel_Red_1, dutyCycle);
        //     delay(7);
        // }
        
        // for (int dutyCycle = 255; dutyCycle >= 0; dutyCycle--) {
        //     ledcWrite(m_channel_Red_1, dutyCycle);
        //     delay(7);
        // }

        delay(125);
    };
}


void LedHandler::CleanUp( void )
{

}