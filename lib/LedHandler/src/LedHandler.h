#ifndef _LED_HANDLER_PLAYER
    #define _LED_HANDLER_PLAYER

    #include "Arduino.h"


    class LedHandler
    {
        public:

            // Constructor.  Only sets pin values.
            LedHandler();
            ~LedHandler();

            bool begin( void );
            bool SetEventGroup (EventGroupHandle_t  *pEventGroup);

        private:
            TaskHandle_t         m_handle;
            EventGroupHandle_t  *m_pEventGroup;

            const double         m_frequency = 5000;
            const uint8_t        m_resolution = 8;

            const uint8_t        m_channel_Red_1 = 0;
            const uint8_t        m_channel_Red_2 = 1;
            const uint8_t        m_channel_Green_1 = 2;
            const uint8_t        m_channel_Green_2 = 3;
            
            //
            void Run( void );
            void CleanUp( void );

            static void TaskFunctionAdapter(void *pvParameters);
    };

#endif