//////////////////////////////////////////////////////////
//  _____        _                    _____      _
// |  __ \      (_)                  / ____|    (_)
// | |  | |_   _ _ _ __   ___ ______| |     ___  _ _ __
// | |  | | | | | | '_ \ / _ \______| |    / _ \| | '_ \ 
// | |__| | |_| | | | | | (_) |     | |___| (_) | | | | |
// |_____/ \__,_|_|_| |_|\___/       \_____\___/|_|_| |_|
//  Code for ESP8266 boards - V2.55
//  © Duino-Coin Community 2019-2021
//  Distributed under MIT License
//////////////////////////////////////////////////////////
//  https://github.com/revoxhere/duino-coin - GitHub
//  https://duinocoin.com - Official Website
//  https://discord.gg/k48Ht5y - Discord
//  https://github.com/revoxhere - @revox
//  https://github.com/JoyBed - @JoyBed
//  https://github.com/kickshawprogrammer - @kickshawprogrammer
//////////////////////////////////////////////////////////
//  If you don't know what to do, visit official website
//  and navigate to Getting Started page. Happy mining!
//////////////////////////////////////////////////////////

// Credit to : Farrukh
// Link to : https://www.youtube.com/watch?v=qCmdUtguwPw&t=321s
// Link to : https://www.youtube.com/watch?v=FuY6BobS-1k&t=0s

// Modify by : Thanormsin.M
// Modify Date : 11-Jul-21
#include <EEPROM.h>

//### Auto WiFi Connect ##################
const char* ssidNm    = "";

const char* ssid2     = "duco-mikrotik";        //<-- ใส่ชือ Wifi รอง ตรงนี ถ้าใช้ WiFi ตัวเดียวก็ให้ใส่เหมือนกันกับ ssid1
const char* password2 = "1234567890";         //<-- ใส่รหัส Wifi รอง ตรงนี

const char* ssid1     = "MY_IOT";        //<-- ใส่ชือ Wifi หลัก
const char* password1 = "0819065291";         //<-- ใส่รหัส Wifi หลัก ตรงนี

//### Other ##################
int lastWiFi = 0;
//int ReCnctWiFi = 1;

const char* ssid     = "MY_IOT";            // Change this to your WiFi SSID
const char* password = "0819065291";        // Change this to your WiFi password

const char* ducouser = "Thanormsin";  // Change this to your Duino-Coin username
const char* rigname  = "ESP32-U03";                     // Change this if you want to display a custom rig name in the Wallet
#define LED_BUILTIN     2                           // Change this if your board has built-in led on non-standard pin (NodeMCU - 16 or 2)
#define WDT_TIMEOUT 60                              // Define watchdog timer seconds

String FirmwareVer = {"2.0"};
#define URL_fw_Version "https://raw.githubusercontent.com/Thanormsin/FwESP32/master/bin_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/Thanormsin/FwESP32/master/fw.bin"

//////////////////////////////////////////////////////////
//  If you're using the ESP32-CAM board or other board
//  that doesn't support OTA (Over-The-Air programming)
//  comment the ENABLE_OTA definition line
//  (#define ENABLE_OTA)
//////////////////////////////////////////////////////////

#define ENABLE_OTA

//////////////////////////////////////////////////////////
//  If you don't want to use the Serial interface comment
//  the ENABLE_SERIAL definition line (#define ENABLE_SERIAL)
//////////////////////////////////////////////////////////

#define ENABLE_SERIAL

#ifndef ENABLE_SERIAL
// disable Serial output
#define Serial DummySerial
static class {
public:
    void begin(...) {}
    void print(...) {}
    void println(...) {}
    void printf(...) {}
} Serial;
#endif

// #include "esp32/sha.h" // Uncomment this line if you're using an older version of the ESP32 core and sha_parellel_engine doesn't work for you
 #include "hwcrypto/sha.h" // If the above still doesn't work, you can try this one
//#include "sha/sha_parallel_engine.h" // Include hardware accelerated hashing library

#include <esp_task_wdt.h> //Include WDT libary
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////
//#include <WiFi.h>
//#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"

#ifdef ENABLE_OTA
#include <ArduinoOTA.h>
#endif




void connect_wifi();
void firmwareUpdate();
int FirmwareVersionCheck();

unsigned long previousMillis = 0; // will store last time LED was updated
unsigned long previousMillis_2 = 0;
const long interval = 15000;
const long mini_interval = 1000;

void repeatedCall() {
  static int num=0;
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    if (FirmwareVersionCheck()) {
      firmwareUpdate();
    }
  }
  if ((currentMillis - previousMillis_2) >= mini_interval) {
    previousMillis_2 = currentMillis;
//    Serial.print("idle loop...");
//    Serial.print(num++);
    Serial.print("Current Active fw version : "); 
    Serial.println(FirmwareVer);
    Serial.println(URL_fw_Bin);
    Serial.println("");
    //Serial.println(FirmwareVer);
   if(WiFi.status() == WL_CONNECTED) 
   {
//    Serial.println("wifi connected");
   }
   else
   {
    //connect_wifi();
    Connect2WiFi();
   }
  }
}

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

Button button_boot = {
  0,
  0,
  false
};
/*void IRAM_ATTR isr(void* arg) {
    Button* s = static_cast<Button*>(arg);
    s->numberKeyPresses += 1;
    s->pressed = true;
}*/

void IRAM_ATTR isr() {
  button_boot.numberKeyPresses += 1;
  button_boot.pressed = true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

TaskHandle_t WiFirec;
TaskHandle_t Task1;
TaskHandle_t Task2;
SemaphoreHandle_t xMutex;


const char * urlPool = "http://51.15.127.80:4242/getPool";
unsigned int share_count = 0; // Share variable
String host = "";
int port = 0;

volatile int wifiStatus = 0;
volatile int wifiPrev = WL_CONNECTED;
volatile bool OTA_status = false;

volatile char ID[23]; // DUCO MCU ID

void SetHostPort(String h, int p)
{
  host = h;
  port = p;
}

void UpdateHostPort(String input) {
  // Thanks @ricaun for the code
  DynamicJsonDocument doc(256);
  deserializeJson(doc, input);

  const char* name = doc["name"];
  const char* host = doc["ip"];
  int port = doc["port"];

  Serial.println("Fetched pool: " + String(name) + " " + String(host) + " " + String(port));
  SetHostPort(String(host), port);
}

String httpGetString(String URL) {
  String payload = "";
  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, URL))
  {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      payload = http.getString();
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  return payload;
}

void UpdatePool() {
  String input = httpGetString(urlPool);
  if (input == "") return;
  UpdateHostPort(input);
}


void WiFireconnect( void * pvParameters ) {
  int n = 0;
  unsigned long previousMillis = 0;
  const long interval = 500;
  esp_task_wdt_add(NULL);
  for(;;) {
    wifiStatus = WiFi.status();
    
    #ifdef ENABLE_OTA
    ArduinoOTA.handle();
    #endif
    
    if (OTA_status)  // If the OTA is working then reset the watchdog.
      esp_task_wdt_reset();
    // check if WiFi status has changed.
    if ((wifiStatus == WL_CONNECTED)&&(wifiPrev != WL_CONNECTED)) {
      esp_task_wdt_reset(); // Reset watchdog timer
      Serial.println(F("\nConnected to WiFi!"));
      Serial.println("Local IP address: " + WiFi.localIP().toString());
      Serial.println();
      UpdatePool();
    }
    else if ((wifiStatus != WL_CONNECTED)&&(wifiPrev == WL_CONNECTED)) {
      esp_task_wdt_reset(); // Reset watchdog timer
      Serial.println(F("\nWiFi disconnected!"));
      WiFi.disconnect();
      Serial.println(F("Scanning for WiFi networks"));
      n = WiFi.scanNetworks(false, true);
      Serial.println(F("Scan done"));
      if (n == 0) {
          Serial.println(F("No networks found. Resetting ESP32."));
          esp_restart();
      } else {
          Serial.print(n);
          Serial.println(F(" networks found"));
          for (int i = 0; i < n; ++i) {
          //Blinking LED
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
    
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
          
          digitalWrite(LED_BUILTIN,HIGH);  
            
          // Print SSID and RSSI for each network found
          Serial.print(i + 1);
          Serial.print(F(": "));
          Serial.print(WiFi.SSID(i));
          Serial.print(F(" ("));
          Serial.print(WiFi.RSSI(i));
          Serial.print(F(")"));
          Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
          delay(10);
        }
      }
      esp_task_wdt_reset(); // Reset watchdog timer
      Serial.println();
      Serial.println(F("Please, check if your WiFi network is on the list and check if it's strong enough (greater than -90)."));
      Serial.println("ESP32 will reset itself after "+String(WDT_TIMEOUT)+" seconds if can't connect to the network");
      Serial.print("Connecting to: " + String(ssid));
      WiFi.reconnect();
    }
    else if ((wifiStatus == WL_CONNECTED)&&(wifiPrev == WL_CONNECTED)) {
      esp_task_wdt_reset(); // Reset watchdog timer
      delay(1000);
    }
    else {
       // Don't reset watchdog timer. If the timer gets to the timeout then it will reset the MCU
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        Serial.print(F("."));
      }
    }
    wifiPrev = wifiStatus;
  }
}

// Task1code
void Task1code( void * pvParameters ) {
  WiFiClient client1;  
  unsigned long Shares1 = 0; // Share variable
  String SERVER_VER = "";
  int buff1size = 0;
  String hash1 = "";
  String job1 = "";
  unsigned int diff1 = 0;
  size_t len = 0;
  size_t final_len = 0;
  unsigned int sizeofjob11 = 100;
  unsigned char* job11 = (unsigned char*)malloc(sizeofjob11 * sizeof(unsigned char));
  byte shaResult1[20];
  unsigned long StartTime1 = 0;
  String hash11 = "";
  unsigned int payloadLenght1 = 0;
  unsigned long EndTime1 = 0;
  unsigned long ElapsedTime1 = 0;
  float ElapsedTimeMiliSeconds1 = 0.0;
  float ElapsedTimeSeconds1 = 0.0;
  float HashRate1 = 0.0;
  String feedback1 = "";
  esp_task_wdt_add(NULL);
  for(;;) {
    if (OTA_status)  // If the OTA is working then reset the watchdog.
      esp_task_wdt_reset();
    while(wifiStatus != WL_CONNECTED){
      delay(1000);
      esp_task_wdt_reset();
    }
    Shares1 = 0; // Share variable
    Serial.println(F("\nCORE1 Connecting to Duino-Coin server..."));
    // Use WiFiClient class to create TCP connection
    client1.setTimeout(1);
    client1.flush();
    yield();
    if (!client1.connect(host.c_str(), port)) {
      Serial.println(F("CORE1 connection failed"));
      //Blinking LED
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);

      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);

      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
      continue;
    }
    Serial.println(F("CORE1 is connected"));
    while(!client1.available()){
      yield();
      if (!client1.connected())
        break;
      delay(10);
    }
    
    SERVER_VER = client1.readString(); // Server sends SERVER_VERSION after connecting
    digitalWrite(LED_BUILTIN, HIGH);   // Turn off built-in led
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);   // Turn on built-in led
    Serial.println("CORE1 Connected to the server. Server version: " + String(SERVER_VER));
    digitalWrite(LED_BUILTIN, HIGH);   // Turn off built-in led
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);   // Turn on built-in led
    
    while (client1.connected()) {
      esp_task_wdt_reset();
      Serial.println("");
      Serial.println("CORE1 Asking for a new job for user: " + String(ducouser)+ "/" + String(rigname)+ "/" + String(host) + "/" + String(port));
      client1.flush();
      client1.print("JOB," + String(ducouser) + ",ESP32"); // Ask for new job
      while(!client1.available()){
        if (!client1.connected())
          break;
        delay(10);
      }
      yield();
      if (!client1.connected())
        break;
      delay(50);
      yield();
      buff1size = client1.available();
      Serial.print(F("CORE1 Buffer size is "));
      Serial.println(buff1size);
      if (buff1size<=10) {
        Serial.println(F("CORE1 Buffer size is too small. Requesting another job."));
        continue;
      }
      hash1 = client1.readStringUntil(','); // Read data to the first peroid - last block hash
      job1 = client1.readStringUntil(','); // Read data to the next peroid - expected hash
      diff1 = client1.readStringUntil('\n').toInt() * 100 + 1; // Read and calculate remaining data - difficulty
      client1.flush();
      job1.toUpperCase();
      const char * c = job1.c_str();
      
      len = job1.length();
      final_len = len / 2;
      memset(job11, 0, sizeofjob11);
      for (size_t i = 0, j = 0; j < final_len; i += 2, j++)
        job11[j] = (c[i] % 32 + 9) % 25 * 16 + (c[i + 1] % 32 + 9) % 25;
      memset(shaResult1, 0, sizeof(shaResult1));

      //Blinking LED
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);

      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
      
      digitalWrite(LED_BUILTIN,HIGH);  
      
      Serial.println("CORE1 Job received: " + String(hash1) + " " + String(job1) + " " + String(diff1));
      StartTime1 = micros(); // Start time measurement
      
      for (unsigned long iJob1 = 0; iJob1 < diff1; iJob1++) { // Difficulty loop
        hash11 = hash1 + String(iJob1);
        payloadLenght1 = hash11.length();
        
        while( xSemaphoreTake( xMutex, portMAX_DELAY ) != pdTRUE );
        esp_sha(SHA1, (const unsigned char*)hash11.c_str(), payloadLenght1, shaResult1);
        xSemaphoreGive( xMutex );
        
        if (memcmp(shaResult1, job11, sizeof(shaResult1)) == 0) { // If result is found
          EndTime1 = micros(); // End time measurement
          ElapsedTime1 = EndTime1 - StartTime1; // Calculate elapsed time
          ElapsedTimeMiliSeconds1 = ElapsedTime1 / 1000; // Convert to miliseconds
          ElapsedTimeSeconds1 = ElapsedTimeMiliSeconds1 / 1000; // Convert to seconds
          HashRate1 = iJob1 / ElapsedTimeSeconds1; // Calculate hashrate
          if (!client1.connected()) {
            Serial.println(F("CORE1 Lost connection. Trying to reconnect"));
            if (!client1.connect(host.c_str(), port)) {
              Serial.println(F("CORE1 connection failed"));
              break;
            }
            Serial.println(F("CORE1 Reconnection successful."));
          }
          client1.flush();
          client1.print(String(iJob1) + "," + String(HashRate1) + ",ESP32 CORE1 Miner v2.63," + String(rigname) + "," + String((char*)ID)); // Send result to server
          Serial.println(F("CORE1 Posting result and waiting for feedback."));
          while(!client1.available()){
            if (!client1.connected()) {
              Serial.println(F("CORE1 Lost connection. Didn't receive feedback."));
              break;
            }
            delay(10);
            yield();
          }
          delay(50);
          yield();
          feedback1 = client1.readStringUntil('\n'); // Receive feedback
          client1.flush();
          Shares1++;
          Serial.println("CORE1 " + String(feedback1) + " share #" + String(Shares1) + " (" + String(iJob1) + ")" + " Hashrate: " + String(HashRate1));
          if (HashRate1 < 4000) {
            Serial.println(F("CORE1 Low hashrate. Restarting"));
            client1.flush();
            client1.stop();
            esp_restart();
          }
          break; // Stop and ask for more work
        }
      }
    }
    Serial.println(F("CORE1 Not connected. Restarting core 1"));
    client1.flush();
    client1.stop();
  }
}

//Task2code
void Task2code( void * pvParameters ) {
  WiFiClient client;
  unsigned long Shares = 0;
  String SERVER_VER = "";
  int buff1size = 0;
  String hash = "";
  String job = "";
  unsigned int diff = 0;
  size_t len = 0;
  size_t final_len = 0;
  unsigned int sizeofjob1 = 100;
  unsigned char* job1 = (unsigned char*)malloc(sizeofjob1 * sizeof(unsigned char));
  byte shaResult[20];
  unsigned long StartTime = 0;
  String hash1 = "";
  unsigned int payloadLength = 0;
  unsigned long EndTime = 0;
  unsigned long ElapsedTime = 0;
  float ElapsedTimeMiliSeconds = 0.0;
  float ElapsedTimeSeconds = 0.0;
  float HashRate = 0.0;
  String feedback = "";
  esp_task_wdt_add(NULL);
  for(;;) {
    if (OTA_status)  // If the OTA is working then reset the watchdog.
      esp_task_wdt_reset();
    while(wifiStatus != WL_CONNECTED){
      delay(1000);
      esp_task_wdt_reset();
    }
    Shares = 0; // Share variable
    
    Serial.println(F("\nCORE2 Connecting to Duino-Coin server..."));
    // Use WiFiClient class to create TCP connection
    client.setTimeout(1);
    client.flush();
    yield();
    if (!client.connect(host.c_str(), port)) {
      Serial.println(F("CORE2 connection failed"));
      //Blinking LED
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);

      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);

      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(150);
      continue;
    }
    Serial.println(F("CORE2 is connected"));
    while(!client.available()){
      yield();
      if (!client.connected())
        break;
      delay(10);
    }
    
    SERVER_VER = client.readString(); // Server sends SERVER_VERSION after connecting
    digitalWrite(LED_BUILTIN, HIGH);   // Turn off built-in led
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);   // Turn on built-in led
    Serial.println("CORE2 Connected to the server. Server version: " + String(SERVER_VER));
    digitalWrite(LED_BUILTIN, HIGH);   // Turn off built-in led
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);   // Turn on built-in led
    
    while (client.connected()) {
      esp_task_wdt_reset();
      Serial.println("");
      Serial.println("CORE2 Asking for a new job for user: " + String(ducouser)+ "/" + String(rigname)+ "/" + String(host) + "/" + String(port));
      client.flush();
      client.print("JOB," + String(ducouser) + ",ESP32"); // Ask for new job
      while(!client.available()){
        if (!client.connected())
          break;
        delay(10);
      }
      yield();
      if (!client.connected())
        break;
      delay(50);
      yield();
      buff1size = client.available();
      Serial.print(F("CORE2 Buffer size is "));
      Serial.println(buff1size);
      if (buff1size<=10) {
        Serial.println(F("CORE2 Buffer size is too small. Requesting another job."));
        continue;
      }
      hash = client.readStringUntil(','); // Read data to the first peroid - last block hash
      job = client.readStringUntil(','); // Read data to the next peroid - expected hash
      diff = client.readStringUntil('\n').toInt() * 100 + 1; // Read and calculate remaining data - difficulty
      client.flush();
      job.toUpperCase();
      const char * c = job.c_str();
      
      len = job.length();
      final_len = len / 2;
      memset(job1, 0, sizeofjob1);
      for (size_t i = 0, j = 0; j < final_len; i += 2, j++)
        job1[j] = (c[i] % 32 + 9) % 25 * 16 + (c[i + 1] % 32 + 9) % 25;
      memset(shaResult, 0, sizeof(shaResult));

      //Blinking LED
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);

      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
    
      digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
      delay(200);
      
      digitalWrite(LED_BUILTIN,HIGH);       
      
      Serial.println("CORE2 Job received: " + String(hash) + " " + String(job) + " " + String(diff));
      StartTime = micros(); // Start time measurement
      
      for (unsigned long iJob = 0; iJob < diff; iJob++) { // Difficulty loop
        hash1 = hash + String(iJob);
        payloadLength = hash1.length();
        
        while( xSemaphoreTake( xMutex, portMAX_DELAY ) != pdTRUE );
        esp_sha(SHA1, (const unsigned char*)hash1.c_str(), payloadLength, shaResult);
        xSemaphoreGive( xMutex );
        
        if (memcmp(shaResult, job1, sizeof(shaResult)) == 0) { // If result is found
          EndTime = micros(); // End time measurement
          ElapsedTime = EndTime - StartTime; // Calculate elapsed time
          ElapsedTimeMiliSeconds = ElapsedTime / 1000; // Convert to miliseconds
          ElapsedTimeSeconds = ElapsedTimeMiliSeconds / 1000; // Convert to seconds
          HashRate = iJob / ElapsedTimeSeconds; // Calculate hashrate
          if (!client.connected()) {
            Serial.println(F("CORE2 Lost connection. Trying to reconnect"));
            if (!client.connect(host.c_str(), port)) {
              Serial.println(F("CORE2 connection failed"));
              break;
            }
            Serial.println(F("CORE2 Reconnection successful."));
          }
          client.flush();
          client.print(String(iJob) + "," + String(HashRate) + ",ESP32 CORE2 Miner v2.63," + String(rigname) + "," + String((char*)ID)); // Send result to server
          Serial.println(F("CORE2 Posting result and waiting for feedback."));
          while(!client.available()){
            if (!client.connected()) {
              Serial.println(F("CORE2 Lost connection. Didn't receive feedback."));
              break;
            }
            delay(10);
            yield();
          }
          delay(50);
          yield();
          feedback = client.readStringUntil('\n'); // Receive feedback
          client.flush();
          Shares++;
          Serial.println("CORE2 " + String(feedback) + " share #" + String(Shares) + " (" + String(iJob) + ")" + " Hashrate: " + String(HashRate));
          if (HashRate < 4000) {
            Serial.println(F("CORE2 Low hashrate. Restarting"));
            client.flush();
            client.stop();
            esp_restart();
          }
          break; // Stop and ask for more work
        }
      }
    }
    Serial.println(F("CORE2 Not connected. Restarting core 2"));
    client.flush();
    client.stop();
  }
}

void setup() {
  pinMode(button_boot.PIN, INPUT);
  attachInterrupt(button_boot.PIN, isr, RISING);
  
  //disableCore0WDT();
  //disableCore1WDT();
  Serial.begin(115200); // Start serial connection
  Serial.println("\n\nDuino-Coin ESP32 Miner v2.63");

  Serial.print("Active firmware version : ");
  Serial.println(URL_fw_Bin);
    //Serial.println(FirmwareVer);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH); 
  
  //connect_wifi();
  Connect2WiFi();
  
//  WiFi.mode(WIFI_STA); // Setup ESP in client mode
//  btStop();
//  WiFi.begin(ssid, password); // Connect to wifi
  
  uint64_t chipid = ESP.getEfuseMac(); // Getting chip ID
  uint16_t chip = (uint16_t)(chipid >> 32); // Preparing for printing a 64 bit value (it's actually 48 bits long) into a char array
  snprintf((char*)ID, 23, "DUCOID%04X%08X", chip, (uint32_t)chipid); // Storing the 48 bit chip ID into a char array.
  
  OTA_status = false;
  
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);
  
  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");
  
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  #ifdef ENABLE_OTA
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
      OTA_status = true;
    })
    .onEnd([]() {
      Serial.println(F("\nEnd"));
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      OTA_status = true;
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
      else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
      else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
      else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
      else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
      OTA_status = false;
      esp_restart();
    });
  
  ArduinoOTA.setHostname(rigname);
  ArduinoOTA.begin();
  #endif
  
  esp_task_wdt_init(WDT_TIMEOUT, true); // Init Watchdog timer
  pinMode(LED_BUILTIN,OUTPUT);
  
  xMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(WiFireconnect, "WiFirec", 10000, NULL, 3, &WiFirec, 0); //create a task with priority 3 and executed on core 0
  delay(250);
  xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 1, &Task1, 0); //create a task with priority 1 and executed on core 0
  delay(250);
  xTaskCreatePinnedToCore(Task2code, "Task2", 10000, NULL, 2, &Task2, 1); //create a task with priority 2 and executed on core 1
  delay(250);

  //ตรวจสอบตอนครั้งแรก
//  FirmwareVersionCheck();
}

void loop() {
  if (button_boot.pressed) { //to connect wifi via Android esp touch app 
    Serial.println("Manual Starting Starting..");
    FirmwareVersionCheck();
    //firmwareUpdate();
    button_boot.pressed = false;
  }
  
  repeatedCall(); 
}

void connect_wifi() {
  Serial.println("Waiting for WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void firmwareUpdate(void) {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    Serial.println("");
    Serial.println("Update Complete. Just Restart");
    delay(2000);
    ESP.restart();
    break;
  }
}

int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  //YYYYY
//  Serial.println(fwurl);
  WiFiClientSecure * client = new WiFiClientSecure;

  if (client) 
  {
    client -> setCACert(rootCACertificate);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
    HTTPClient https;

    if (https.begin( * client, fwurl)) 
    { // HTTPS  
      //Serial.print("[HTTPS] GET...\n");
      //Serial.println("");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {       
        payload = https.getString(); // save received version    
      } else {
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }
      
  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version :%s\n", FirmwareVer);
      return 0;
    } 
    else 
    {

      disableCore0WDT();
      disableCore1WDT();
      Serial.print("New firmware detected : ");
      Serial.println(payload);
      Serial.println("Downloading and install new firmware");
      Serial.println(URL_fw_Bin);

//      Serial.print("New firmware detected Version : ");
//      Serial.println(payload);
      return 1;
    }
  } 
  return 0;  
}

/*
////// EEPROM Setup ////////
void GetAcctName(){
//  Serial.println("");
//  Serial.println("------------------Get Account Name ------------------"); 
  // อ่าน AcctName
  eeprom_acct = "";
  for(int i=0;i< 20;i++){ 
//    Serial.print(i);
//    Serial.print(" GAcctNm ");
//    Serial.println(char(EEPROM.read(i))); 
  
    if(EEPROM.read(i) > 20)eeprom_acct += char(EEPROM.read(i));
  }  

  USERNAME = eeprom_acct;
  Serial.println(USERNAME); 
 
}

void GetBoardName(){
  // อ่าน BrdNm
  eeprom_brd = "";
  for(int i=20;i< 40;i++){
   if(EEPROM.read(i) > 0){
//      Serial.print(i);
//      Serial.print(" GBrdNm ");
//      Serial.println(char(EEPROM.read(i)));
    
      eeprom_brd += char(EEPROM.read(i));
   }
  }

  RIG_IDENTIFIER = eeprom_brd;
  Serial.println(RIG_IDENTIFIER); 
}

void SaveName(){  
  Serial.println("");
  Serial.println("------------------Start Save ------------------");
  // CLEAR EEPROM /////////
  for(int i=0;i < 20;i++)
  {
     EEPROM.write(i, 0);
  }
 
  Serial.print("AcctName : ");
  Serial.println(AcctName);  

  // เขียน AcctName
  for(int i=0;i < AcctName.length();i++)
  {
//    Serial.print(i);
//    Serial.print(" SvAcctNm ");
//    Serial.println(AcctName[i]);
    
    EEPROM.write(i, AcctName[i]); 
  }
  USERNAME = AcctName;

  // เคลียร์พื้นที่ก่อน เขียน BrdNm
  for(int i=20;i < 40;i++)
  {
   EEPROM.write(i, 0);
  }

  // เขียน BrdNm
//  Serial.println("------------------Show : BrdNm ------------------"); 
  Serial.print("BoardName : ");
  Serial.println(BrdNm); 
  
  x = 0;
  for(int i=20;i < 40;i++)
  {
//    Serial.print(i);
//    Serial.print(" SvBrdNm ");
//    Serial.println(BrdNm[x]); 

    if(x < BrdNm.length())EEPROM.write(i, BrdNm[x]); //Write one by one with starting address of 0x0F
    x++;
  }
  RIG_IDENTIFIER = BrdNm;
  
  EEPROM.commit();
   
  Serial.println("------------------End Save ------------------");
}

*/
////// ตั้งค่า 2 WiFi //////
void Connect2WiFi()
{
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_OFF);

/*
          //Blinking LED
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
    
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
          
          digitalWrite(LED_BUILTIN,HIGH);  
    
//      Serial.print("O");
*/      
      //delay(250);
      
      WiFi.mode(WIFI_STA);
      if (lastWiFi == 1) {
        Serial.print("Connecting WiFi#2 :");
        Serial.println(ssid2);
        WiFi.begin(ssid2, password2);
        
        lastWiFi = 2;
        ssid = ssid2;   
    
      } else {        
        Serial.print("Connecting WiFi#1 :");
        //WiFi.begin(ssid1, password1);
        
        Serial.println("Start Connect to WiFi"); 
        Serial.println(ssid1);
        WiFi.begin(ssid1, password1);
        
        lastWiFi = 1;
        ssid = ssid1;  
      } 
    
      unsigned long timeout = millis();
      while (WiFi.status() != WL_CONNECTED) {
         if (millis() - timeout > 20000) {
            Serial.println("");
            Serial.println(">>> Connecting Wait Timeout !");
            break;
         }
//         delay(500);
          //Blinking LED
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
    
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
        
          digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));
          delay(150);
          
          digitalWrite(LED_BUILTIN,HIGH);  
 ;
      }
    }
    Serial.println("");
    Serial.print("Connected to WIFi: ");
//    Serial.print("SSID: ");
    Serial.println(ssidNm);
    Serial.print("IP address: ");
    Serial.print(WiFi.localIP());     
    Serial.println(""); 

    delay(5000);
}
