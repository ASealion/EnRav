#ifndef _USER_INTERFACE_H
    #define _USER_INTERFACE_H

    #include "Arduino.h"

    #include "MFRC522.h"

    class UserInterface
    {

        #define     INFORMATION_BLOCK_MIFARE_1K         4
        #define     INFORMATION_BLOCK_MIFARE_ULTRA      8

        #define     TARGET_BLOCK_MIFARE_1K              8
        #define     TARGET_BLOCK_MIFARE_ULTRA

        public:
            // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
            UserInterface(uint8_t cs, uint8_t rst);
            ~UserInterface();

            void begin( QueueHandle_t *pPlayerCommandQueue );

        private:
                    // this object stores nfc tag data
            typedef struct {
                union {
                    struct {
                        struct {
                            uint32_t    Cookie;             // 4 byte
                            uint32_t    Version;            // 4 byte
                        } Header;
                        struct {
                            int16_t     TargetLength;       // 2 byte
                            uint8_t     Configuration;      // 1 byte
                            uint8_t     LastListPostion;    // 1 Byte
                            uint32_t    LastFilePosition;   // 4 byte
                        } MetaData;
                    }Entry;                             // complete 16 byte
                    uint8_t             Raw16[16];
                    uint8_t             Raw4[4][4];
                }Information;

                char                    *pTarget;
                bool                    TagValid;
            } nfcTag_s;

            TaskHandle_t            m_handle;
            QueueHandle_t           *m_pPlayerQueue;
            MFRC522                 *m_pRfReader;
            MFRC522::MIFARE_Key     m_MFRC522Key;
            nfcTag_s                m_NfcTag;

            static const    uint32_t    magicKey = 0x13374258;

            //
            void Run( void );
            void CleanUp( void );

            void DumpByteArray(const char *header, byte *buffer, byte bufferSize);
            
            static void TaskFunctionAdapter(void *pvParameters);

            bool ReadInformationFromTag( void );
            bool WriteInformationToTag( void );

    };

#endif