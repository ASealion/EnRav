#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"
#include "mp3player.h"
#include "UserInterface.h"

//----------------------------
#include <WebServer.h>
#include <ESPmDNS.h>
//----------------------------

#ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
#else
    static const char *TAG = "main";
#endif

#include "credentials.h"

#ifndef __CREDENTIALS__H
    String ssid =     "WLAN";
    String password = "xxxxxxxxxxxxxxxx";
#endif

// Digital I/O used
#define SPI_CLK         14    
#define SPI_MISO         4
#define SPI_MOSI        15

#define SDCARD_CS       13

#define VS1053_CS       25
#define VS1053_DCS      26
#define VS1053_DREQ     32

#define MFRC522_RST     19
#define MFRC522_CS      21


int volume=15;

UserInterface   myInterface(MFRC522_CS, MFRC522_RST);
Mp3player       MyPlayer(VS1053_CS, VS1053_DCS, VS1053_DREQ);

QueueHandle_t   PlayerCommandQueue;

//----------------------------
WebServer server(80);

const char* host = "EnRav";

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    ESP_LOGD(TAG, "Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void printDirectory() {
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
#ifdef ESP8266
    output += entry.name();
#else
    // Ignore '/' prefix
    output += entry.name()+1;
#endif
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 // Send zero length chunk to terminate the HTTP body
 server.sendContent("");
 dir.close();
}

File uploadFile;

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    ESP_LOGD(TAG, "Upload: START, filename: %s", upload.filename.c_str());
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    ESP_LOGD(TAG, "Upload: WRITE, Bytes: %u", upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile) uploadFile.close();
    ESP_LOGD(TAG, "Upload: END, Size: %u", upload.totalSize);
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
#ifdef ESP8266
      file.write((const char *)0);
#else
      // TODO Create file with 0 bytes???
      file.write(NULL, 0);
#endif
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}


void handleNotFound(){
  if(loadFromSdCard(server.uri())) return;
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  ESP_LOGD(TAG, "%s", message.c_str());
}

//----------------------------





//The setup function is called once at startup of the sketch
void setup() {

    PlayerCommandQueue = xQueueCreate( 5, sizeof( PlayerControlMessage_s ) );

    Serial.begin(115200);

    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI);

    myInterface.begin(&PlayerCommandQueue);

    SD.begin(SDCARD_CS);

    MyPlayer.begin(&PlayerCommandQueue);

//----------------------------
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
      // Wait for connection
    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED && i++ < 20) //wait 10 seconds
    {
        delay(500);
    }

    if(i == 21)
    {
        ESP_LOGD(TAG, "Could not connect to %s", ssid.c_str());
        while(1) delay(500);
    }

    ESP_LOGI(TAG, "Connected! IP address: %s", WiFi.localIP().toString().c_str());

    if (MDNS.begin(host)) 
    {
        MDNS.addService("http", "tcp", 80);
        ESP_LOGD(TAG, "MDNS responder started");
        ESP_LOGD(TAG, "You can now connect to http://%s.local", host);
    }


    server.on("/list", HTTP_GET, printDirectory);
    server.on("/edit", HTTP_DELETE, handleDelete);
    server.on("/edit", HTTP_PUT, handleCreate);
    server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);
    server.onNotFound(handleNotFound);

    server.begin();
    ESP_LOGI(TAG, "HTTP server started");

//----------------------------

}


// The loop function is called in an endless loop
void loop()
{
    server.handleClient();
    // delay(100);

}
