#ifndef _MP3_PLAYER
    #define _MP3_PLAYER

    #include "Arduino.h"
    #include "SPI.h"
    #include "SD.h"
    #include "FS.h"

    #include "vs1053_ext.h"


    class Mp3player
    {
        public:
            typedef enum {
                CMD_UNKNOWN,
                CMD_PLAY_FILE,
                CMD_RESUME_FILE,
                CMD_STOP,
                CMD_VOL_UP,
                CMD_VOL_DOWN,
            } PlayerCommand_e;

            typedef struct {
                PlayerCommand_e Command;
                String         *pFileToPlay;
            } PlayerControlMessage_s;

            // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
            Mp3player(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin);
            ~Mp3player();

            void            begin( QueueHandle_t *commandQueue );

            void            SetSystemFlagGroup(EventGroupHandle_t eventGroup);
            QueueHandle_t   *getQueue( void );

        private:
            TaskHandle_t        m_handle;
            QueueHandle_t       *m_pPlayerQueue;
            EventGroupHandle_t  m_SystemFlagGroup;
            VS1053              *m_pPlayer;

            uint8_t             m_volume;

            //
            void Run( void );
            void CleanUp( void );

            static void TaskFunctionAdapter(void *pvParameters);
    };

#endif