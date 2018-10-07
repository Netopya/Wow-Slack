/************************************************* ************************************************** ******
* OPEN-SMART Red Serial MP3 Player Lesson 1: Play a song
NOTE!!! First of all you should download the voice resources from our google drive:
https://drive.google.com/drive/folders/0B6uNNXJ2z4CxaFVzZEZZVTR5Snc?usp=sharing


Then unzip it and find the 01 and 02 folder and put them into your TF card (should not larger than 32GB). 

* You can learn how to play a song with its index in the TF card.
*
* The following functions are available:
*
/--------basic operations---------------/
mp3.play();
mp3.pause();
mp3.nextSong();
mp3.previousSong();
mp3.volumeUp();
mp3.volumeDown();
mp3.forward();    //fast forward
mp3.rewind();     //fast rewind
mp3.stopPlay();  
mp3.stopInject(); //when you inject a song, this operation can stop it and come back to the song befor you inject
mp3.singleCycle();//it can be set to cycle play the currently playing song 
mp3.allCycle();   //to cycle play all the songs in the TF card
/--------------------------------/

mp3.playWithIndex(int8_t index);//play the song according to the physical index of song in the TF card

mp3.injectWithIndex(int8_t index);//inject a song according to the physical index of song in the TF card when it is playing song.

mp3.setVolume(int8_t vol);//vol is 0~0x1e, 30 adjustable level

mp3.playWithFileName(int8_t directory, int8_t file);//play a song according to the folder name and prefix of its file name
                                                            //foler name must be 01 02 03...09 10...99
                                                            //prefix of file name must be 001...009 010...099

mp3.playWithVolume(int8_t index, int8_t volume);//play the song according to the physical index of song in the TF card and the volume set

mp3.cyclePlay(int16_t index);//single cycle play a song according to the physical index of song in the TF

mp3.playCombine(int16_t folderAndIndex[], int8_t number);//play combination of the songs with its folder name and physical index
      //folderAndIndex: high 8bit is folder name(01 02 ...09 10 11...99) , low 8bit is index of the song
      //number is how many songs you want to play combination

About SoftwareSerial library:
The library has the following known limitations:
If using multiple software serial ports, only one can receive data at a time.

Not all pins on the Mega and Mega 2560 support change interrupts, so only the following can be used for RX: 
10, 11, 12, 13, 14, 15, 50, 51, 52, 53, A8 (62), A9 (63), A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69).

Not all pins on the Leonardo and Micro support change interrupts, so only the following can be used for RX: 
8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).
On Arduino or Genuino 101 the current maximum RX speed is 57600bps.
On Arduino or Genuino 101 RX doesn't work on Pin 13.

Store: dx.com/440175
https://open-smart.aliexpress.com/store/1199788

************************************************** **************************************************/
#include <SoftwareSerial.h>
#include "RedMP3.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>

#define SLACK_BOT_TOKEN "#"
#define WIFI_SSID "#"
#define WIFI_PASSWORD "#"

// if Slack changes their SSL fingerprint, you would need to update this:
#define SLACK_SSL_FINGERPRINT "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3" 

#define WORD_SEPERATORS "., \"'()[]<>;:-+&?!\n\t"

#define MP3_RX 12//RX of Serial MP3 module connect to D7 of Arduino
#define MP3_TX 13//TX to D8, note that D8 can not be used as RX on Mega2560, you should modify this if you donot use Arduino UNO

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

MP3* mp3;

int8_t indexfoo  = 0x01;//the first song in the TF card
int8_t volume = 0x10;//0~0x1e (30 adjustable level)

long nextCmdId = 1;
bool connected = false;
unsigned long lastPing = 0;

StaticJsonBuffer<20480> jsonBuffer;

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  updateStatus(0);
  delay(3000);

  updateStatus(1);
  mp3 = new MP3(MP3_RX, MP3_TX);
  delay(1000);

  updateStatus(2);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Starting Up");
 
  updateStatus(3);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  updateStatus(4);
}

void processSlackMessage(const char *message) {
  char *nextWord = NULL;
  Serial.println("processing message");

  char *payload = strdup(message);

  for (nextWord = strtok(payload, WORD_SEPERATORS); nextWord; nextWord = strtok(NULL, WORD_SEPERATORS)) {
    if (strcasecmp(nextWord, "wow") == 0) {
      indexfoo++;
      digitalWrite(LED_BUILTIN, indexfoo % 2);  
      mp3->playWithVolume(indexfoo,volume);
      // WOW! 
    }
  }

  free(payload);
}

void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);

      JsonObject& root = jsonBuffer.parseObject((char*)payload);
      const char* messageType = root["type"];
      Serial.printf("mesage type: %s\n", messageType); 

      /*char json[] = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
      JsonObject& root = jsonBuffer.parseObject(json);
      const char* sensor = root["sensor"];
      Serial.println(sensor);*/

      if (strcmp(messageType, "message") == 0) {
        const char* messageContent = root["text"];
        processSlackMessage(messageContent);
      }
      
      // TODO: process message
      break;
  }
}

bool connectToSlack() {
  updateStatus(3);
  
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  updateStatus(4);
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  Serial.printf("HTTP GET Code %d\n", httpCode);

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  updateStatus(5);
  
  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);

  updateStatus(6);
  
  return true;
}

void updateStatus(int led){
  digitalWrite(LED_BUILTIN, led % 2);  
}


void loop()
{
  webSocket.loop();

  if (connected) {
    // Send ping every 5 seconds, to keep the connection alive
    if (millis() - lastPing > 5000) {
      sendPing();
      lastPing = millis();
    }
  } else {
    // Try to connect / reconnect to slack
    connected = connectToSlack();
    if (!connected) {
      delay(500);
    }
  }
}
