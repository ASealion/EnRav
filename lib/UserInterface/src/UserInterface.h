#ifndef _USER_INTERFACE_H
    #define _USER_INTERFACE_H

    #include "Arduino.h"

    #include "MFRC522.h"

    // this object stores nfc tag data
    __packed typedef struct {
        union {
            struct {
                struct {
                    uint32_t    Cookie;         // 4 byte
                    uint8_t     Version;        // 1 byte
                } Header;
                struct {
                    uint8_t     Shuffle;        // 1 byte
                    uint8_t     Repeat;         // 1 byte
                    uint8_t     Spare[9];       // 9 byte
                } MetaData;
            }Entry;                             // complete 16 byte
            uint8_t             Raw[16];
        }Information;

        //we will use up to 80 byte to store the path for the data file (playlist or mp3)
        union {
            char    DataPath[5*16];
            uint8_t Raw[5][16];
        } Target;

    } nfcTagObject_s;

    class UserInterface
    {
        public:
            // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
            UserInterface(uint8_t cs, uint8_t rst);
            ~UserInterface();

            void begin( void );

            void setQueue( QueueHandle_t *pQueue );

        private:
            TaskHandle_t    m_handle;
            QueueHandle_t   *m_pPlayerQueue;
            MFRC522         *m_pRfReader;

            //
            void Run( void );
            void CleanUp( void );

            void dump_byte_array(byte *buffer, byte bufferSize);
            static void TaskFunctionAdapter(void *pvParameters);
    };

#endif