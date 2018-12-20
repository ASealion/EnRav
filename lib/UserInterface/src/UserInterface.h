#ifndef _USER_INTERFACE_H
    #define _USER_INTERFACE_H

    #include "Arduino.h"    

    #include "cardHandler.h"

    #include "mp3player.h"

    class UserInterfaceCommand {
        enum class Command { SetVolume, WriteCard } m_command;
    };


    class UserInterface
    {
        public:
            typedef enum {
                CMD_UNKNOWN,
                CMD_SET_VOLUME,
                CMD_VOLUME_UP, 
                CMD_VOLUME_DOWN,
                
                CMD_WRITE_CARD,                
            } InterfaceCommand_e;

            typedef struct {
                InterfaceCommand_e      Command;
                void                    *pData;
            } InterfaceCommandMessage_s;


        public:        
            UserInterface();
            ~UserInterface();

            void setPlayerCommandQueue(QueueHandle_t *pCommandQueue);

            void begin(void);

            QueueHandle_t *getInterfaceCommandQueue(void);


        private:
            enum class RfidCardStatus { NoCard, ValidCard, UnknownCard };

            RfidCardStatus          m_CardStatus;           // our interal card status
            CardData                m_CardData;             // the settings from or for the RDIF card 

            // 
            CardHandler             m_CardHandler;          // the class that handels all RFID card communication
            CardSerialNumber        m_CardSerialNumber;     // the last read Uid                        
            uint32_t                m_CardTimestamp;        // 

            // our connection to the MP3 task
            QueueHandle_t           *m_pPlayerQueue;
            
            // out own task and command stuff
            TaskHandle_t            m_handle;
            QueueHandle_t           m_InterfaceCommandQueue;

            //internal functions
            void run( void );
            void cleanUp( void );
            
            static void TaskFunctionAdapter(void *pvParameters);

            uint32_t TimeElapsed(uint32_t TimeStamp);

    };

#endif
