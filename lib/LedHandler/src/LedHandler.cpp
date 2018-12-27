#include "LedHandler.h"

#include "pinout.h"

LedHandler::LedHandler( )
{
    m_pEventGroup = NULL;

    //prepare the pwm channels
    ledcSetup(m_channel_Red_1, m_frequency, m_resolution);
    ledcAttachPin(LED_RED_1, m_channel_Red_1);
}

LedHandler::~LedHandler()
{

}


bool LedHandler::begin( void )
{
    bool result = false;

    ESP_LOGD(TAG, "Start LED Task");

    //check the internal links
    if (m_pEventGroup != NULL)
    {
        
        //create the task that will handle the playback
        xTaskCreate(
                        TaskFunctionAdapter,        /* Task function. */
                        "LED Handler",       	    /* String with name of task. */
                        4 * 128,                    /* Stack size in words. */
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


bool LedHandler::SetEventGroup (EventGroupHandle_t  *pEventGroup)
{
    bool result = true;

    m_pEventGroup = pEventGroup;

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

    while (true)
    {
          for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++) {
            ledcWrite(m_channel_Red_1, dutyCycle);
            delay(7);
        }
        
        for (int dutyCycle = 255; dutyCycle >= 0; dutyCycle--) {
            ledcWrite(m_channel_Red_1, dutyCycle);
            delay(7);
        }

        delay(50);
    };
}


void LedHandler::CleanUp( void )
{

}