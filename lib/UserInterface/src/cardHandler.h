#ifndef __CARD_HANDLER_H
    #define __CARD_HANDLER_H

    #include "Arduino.h"

     #include "MFRC522.h"


    class CardData 
    {
        public:
            String      m_fileName;
            uint32_t    m_PlaylistPosition;
            uint32_t    m_TrackPosition;

            bool        m_valid;

            bool        GetValid() {
                return m_valid;
            }


    };

    class CardSerialNumber
    {
        public:
            uint32_t    SerialNumberLength;
            uint8_t     SerialNumber[10];
            
            String      toString(void) {
                String result = String(SerialNumber[0]);
                for (uint32_t counter = 1; counter < sizeof(SerialNumber); counter++)
                {
                    result += ' ' + String(SerialNumber[counter]);
                }
                return result;
            }
    };

    class CardHandler
    {
        public:
            CardHandler();
            CardHandler(MFRC522 *pCardReader);
            ~CardHandler();

            void        connectCardReader(MFRC522 *pCardReader);

            bool        IsNewCardPresent( void );
            bool        IsCardPresent(CardSerialNumber *pActualCardSerial);

            bool        GetCardSerial(CardSerialNumber *pActualCardSerial);
            
            bool        ReadCardInformation(CardData *pTarget);
            bool        WriteCardInformation(CardData *pSource);
            
            void        StopCommunication(void);


        private:

            typedef union 
            {
                struct {
                    struct {
                        uint32_t    Cookie;             // 4 byte
                        uint32_t    Version;            // 4 byte
                    } Header;
                    struct {
                        uint16_t    FileNameLength;     // 2 byte
                        uint8_t     Configuration;      // 1 byte
                        uint8_t     LastListPostion;    // 1 Byte
                        uint32_t    LastFilePosition;   // 4 byte
                    } MetaData;
                }Entry;                                 // complete 16 byte

                uint8_t             Raw[16];
            } CardDataBlock_s;

            //some internal configuration
            const uint32_t          INFORMATION_BLOCK_MIFARE_1K     =  4;
            const uint32_t          INFORMATION_BLOCK_MIFARE_ULTRA  =  8;
            const uint32_t          TARGET_BLOCK_MIFARE_1K          =  8;
            const uint32_t          TARGET_BLOCK_MIFARE_ULTRA       = 16;

            const uint32_t          INFORMATION_BLOCK_SIZE          =  16;

            const uint32_t          INFORMATION_BLOCK__MAGIC_KEY    = 0x13374258;

            MFRC522                 *m_pRfReader;
            MFRC522::MIFARE_Key     m_MFRC522Key;

    };

#endif
