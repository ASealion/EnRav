#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"
#include "mp3player.h"
#include "UserInterface.h"
#include "LedHandler.h"


#include "pinout.h"

#include "SimpleCLI.h"
using namespace simplecli;

#ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"    
#else
    static const char *TAG = "main";
#endif

#if __has_include("credentials.h")
    #include "credentials.h"
#endif

#ifndef __CREDENTIALS__H
    String ssid =     "WLAN";
    String password = "xxxxxxxxxxxxxxxx";
#endif


UserInterface   myInterface;
Mp3player       MyPlayer(VS1053_CS, VS1053_DCS, VS1053_DREQ);

QueueHandle_t       PlayerCommandQueue;
QueueHandle_t       *pCommandInterfaceQueue;
EventGroupHandle_t  SystemFlagGroup;

LedHandler          MyLedHandler;


//
SimpleCLI           *pCli;          // pointer to command line handler
String              NewCommand;     // string to collect characters from the input

String Version = "EnRav 0.20.0";

//The setup function is called once at startup of the sketch
void setup() {

    //prepare the internal inter task communication channels
    PlayerCommandQueue = xQueueCreate( 5, sizeof( Mp3player::PlayerControlMessage_s ) );
    SystemFlagGroup    = xEventGroupCreate();

    Serial.begin(115200);

    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI);

    //prepare the user interface    
    myInterface.setPlayerCommandQueue(&PlayerCommandQueue);
    pCommandInterfaceQueue = myInterface.getInterfaceCommandQueue();
    myInterface.begin();

    SD.begin(SDCARD_CS);

    MyPlayer.SetSystemFlagGroup(SystemFlagGroup);
    MyPlayer.begin(&PlayerCommandQueue);

    MyLedHandler.SetEventGroup(SystemFlagGroup);
    MyLedHandler.begin();
    
    // WiFi.disconnect();
    // WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid.c_str(), password.c_str());
    // while (WiFi.status() != WL_CONNECTED) delay(1500);

    CommandLine_create();

    Serial.println(Version);
}


// The loop function is called in an endless loop
void loop()
{
    // read serial input
    while(Serial.available()){
        char tmp;
        
        if (Serial.readBytes(&tmp, 1) >= 1) 
        {
            Serial.print(tmp);

            NewCommand += String(tmp);

            if (tmp == '\n')
            {
               pCli->parse(NewCommand); 
               NewCommand = String();
               Serial.print("> ");
            }
        }
    }
}


void CommandLine_create(void)
{
    // =========== Create CommandParser =========== //
    pCli = new SimpleCLI();

    // when no valid command could be found for given user input
    pCli->onNotFound = [](String str) {
                          Serial.println("\"" + str + "\" not found");
                      };
    // ============================================ //

    // =========== Add stop command ========== //
    pCli->addCmd(new EmptyCmd("help", [](Cmd* cmd) {
        
        Serial.println("EnRav Audio player (by M. Reinecke)");
        Serial.println("-----------------------------------");
        Serial.println("available commands:");
        Serial.println("- version               : show firmware version");
        Serial.println("");
        Serial.println("- play <filename>       : start playing the given file name");
        Serial.println("                           from the beginning (must be a mp3 or m3u file)");
        Serial.println("- resume <filename>     : start playing the given file name");
        Serial.println("                           from previous position (must be a mp3 or m3u file)");
        Serial.println("- stop                  : stops the actual playback");
        Serial.println("");
        Serial.println("- volume <1..100>       : set volume to level");
        Serial.println("  volume up             : increase volume by 5 steps");
        Serial.println("  volume down           : decrease volume by 5 steps");
        Serial.println("");
        Serial.println(" - write <filename>     : setup RFID card with the given parameters");
    }));
    // ======================================== //

    // =========== Add version info command ========== //
    pCli->addCmd(new Command("version", [](Cmd* cmd) {
        Serial.println(Version);
    }));
    // ======================================== //

    // =========== Add play command ========== //
    pCli->addCmd(new SingleArgCmd("play", [](Cmd* cmd) {        
        String *pFileName = new String(cmd->getValue(0));

        UserInterface::InterfaceCommandMessage_s newMessage =   { 
                                                                .Command    = UserInterface::CMD_PLAY_FILE,
                                                                .pData      = pFileName 
                                                                };
        // the message is copied to the queue, so no need for the original one :)
        if (!xQueueSend( *pCommandInterfaceQueue, &newMessage, ( TickType_t ) 0 ) )
        {
            ESP_LOGE(TAG, "Send to queue failed");
            delete pFileName;
        }
    }));
    // ======================================== //

    // =========== Add resume command ========== //
    pCli->addCmd(new SingleArgCmd("resume", [](Cmd* cmd) {        
        String *pFileName = new String(cmd->getValue(0));

        UserInterface::InterfaceCommandMessage_s newMessage =   { 
                                                                .Command    = UserInterface::CMD_RESUME_FILE,
                                                                .pData      = pFileName 
                                                                };
        // the message is copied to the queue, so no need for the original one :)
        if (!xQueueSend( *pCommandInterfaceQueue, &newMessage, ( TickType_t ) 0 ) )
        {
            ESP_LOGE(TAG, "Send to queue failed");
            delete pFileName;
        }
    }));
    // ======================================== //

    // =========== Add stop command ========== //
    pCli->addCmd(new EmptyCmd("stop", [](Cmd* cmd) {
        UserInterface::InterfaceCommandMessage_s newMessage = { .Command = UserInterface::CMD_PLAY_STOP };

        // the message is copied to the queue, so no need for the original one :)
        if (!xQueueSend( *pCommandInterfaceQueue, &newMessage, ( TickType_t ) 0 ) )
        {
            ESP_LOGE(TAG, "Send to queue failed");
        }
    }));
    // ======================================== //

    // =========== Add volume command ========== //
    pCli->addCmd(new SingleArgCmd("volume", [](Cmd* cmd) {     
        UserInterface::InterfaceCommandMessage_s newMessage = { .Command = UserInterface::CMD_UNKNOWN};   
        String detail = String(cmd->getValue(0));
        
        if (detail.equalsIgnoreCase("UP"))
        {
            newMessage.Command  = UserInterface::CMD_VOLUME_UP;
            newMessage.pData    = NULL;
        }
        else if (detail.equalsIgnoreCase("DOWN"))
        {
            newMessage.Command  = UserInterface::CMD_VOLUME_DOWN;
            newMessage.pData    = NULL;
        }
        
        if (newMessage.Command != UserInterface::CMD_UNKNOWN)
        {
            // the message is copied to the queue, so no need for the original one :)
            if (!xQueueSend( *pCommandInterfaceQueue, &newMessage, ( TickType_t ) 0 ) )
            {
                ESP_LOGE(TAG, "Send to queue failed");
            } 
        }  
    }));
    // ======================================== //

    // =========== Add write command ========== //
    Command* writeCard = new Command("write", [](Cmd* cmd) {        
        String fileName = cmd->getValue(0);
        String resume   = cmd->getValue(1);
        // bool resume = cmd->isSet("r");
        // long volume = cmd->getValue("v").toInt();
        

        ESP_LOGI(TAG, "Prepare card for file \"%s\"%s", fileName.c_str(), (resume.equalsIgnoreCase("resume"))?" with resume":"");
        // ESP_LOGI(TAG, "Prepare card for file \"%s\"%s volume %ld", fileName.c_str(), resume?" with resume":"", volume);

        if (fileName.length() > 0) 
        {
            UserInterface::InterfaceCommandMessage_s newMessage;
            CardData  *pNewCard = new CardData();

            //save the data to the stucture
            pNewCard->m_fileName    = fileName;
            pNewCard->m_Volume      = 0;
            pNewCard->m_Resumeable  = (resume.equalsIgnoreCase("resume"))?true:false;

            //and fill the message to the interface
            newMessage.Command    = UserInterface::CMD_CARD_WRITE;
            newMessage.pData      = pNewCard ;

            // the message is copied to the queue, so no need for the original one :)
            if (!xQueueSend( *pCommandInterfaceQueue, &newMessage, ( TickType_t ) 0 ) )
            {
                ESP_LOGE(TAG, "Send to queue failed");
                delete pNewCard;
            }            
        } 
        else 
        {
            Serial.println("Illegal parameter (FileName)");
        }
    });    
    writeCard->addArg(new AnonymReqArg());
    // writeCard->addArg(new EmptyArg("r"));
    // writeCard->addArg(new OptArg("write v", "15"));
    writeCard->addArg(new AnonymOptArg());
    pCli->addCmd(writeCard);
    // ======================================== //

    // =========== Add change log level command ========== //
    pCli->addCmd(new SingleArgCmd("log", [](Cmd* cmd) {  
        String data = cmd->getValue(0);

        if (data.equalsIgnoreCase("DEBUG"))
        {
            esp_log_level_set("*", ESP_LOG_DEBUG);
        } 
        else if (data.equalsIgnoreCase("VERBOSE"))
        {
            esp_log_level_set("*", ESP_LOG_VERBOSE);
        } 
        else if (data.equalsIgnoreCase("WARNING"))
        {
            esp_log_level_set("*", ESP_LOG_WARN);
        } 
        else if (data.equalsIgnoreCase("INFO"))
        {
            esp_log_level_set("*", ESP_LOG_INFO);
        }
    }));
    // ======================================== //    

    // =========== Add sleep command ========== //
    pCli->addCmd(new EmptyCmd("sleep", [](Cmd* cmd) {     

        esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0); //1 = High, 0 = Low

        digitalWrite(MFRC522_RST, LOW);

        esp_sleep_enable_timer_wakeup(60 * 1000000);

        esp_deep_sleep_start();
    }));
    // ======================================== //
}
