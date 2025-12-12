/*
Adrian Rosoga
17 Nov 2025 - Initial commit after code cleanup

Based on "ESP32-CAM Telegram Photo Bot" tutorial from Random Nerd Tutorials
https://RandomNerdTutorials.com/telegram-esp32-cam-photo-arduino/
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>

#include "ntp_time.h"
#include "credentials.h"    // WIFI and Telegram credentials - sample below

/*
Sample credentials file:

#include <String.h>

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const String CHAT_ID = "CHAT_ID";  // Use @myidbot to find out the chat ID of an individual or a group

// Telegram Bot Token
const String BOTtoken_1 = "0123456789:M0RVLDOTxWS5toiGTkgosVT-g9ecyfzMpQE";
const String BOTtoken_2 = "0123456789:M0RVLDAlHyMt9mX0wmokAGjQB4LZDOKDEHc";
*/


bool sendPhoto = false;

WiFiClientSecure clientTCP;
String BOTtoken = "DummyBotToken";             // Will be set in setup()
UniversalTelegramBot bot(BOTtoken, clientTCP); // Will be set in setup()

int jpeg_quality = 10; // 10 was default, 0-63 lower number means higher quality

#define FLASH_LED_PIN 4
bool flashState = LOW;

const int HOUR_TO_SEND_PHOTO = 6;
int brightness_g = 255;   // Flash LED brightness (0-255) - This default is too bright

//Checks for new Telegram messages every 1 second.
const int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  //init with high specs to pre-allocate larger buffers
  if(psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = jpeg_quality;  // 0-63 lower number means higher quality
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  // 0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // Take black and white pictures
  sensor_t * s = esp_camera_sensor_get();
  s->set_special_effect(s, 2); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
}


int get_brightness(const String &text, char delimiter) {
  int brightness = 0;
  if (text.indexOf(delimiter) != -1) {
    int index = text.indexOf(delimiter);
    int start = text.indexOf(" ", index);
    int end = text.indexOf(" ", start + 1);
    if (end == -1) {
      end = text.length();
    }
    String brightnessStr = text.substring(start + 1, end);
    brightness = brightnessStr.toInt();
  }
  return brightness;
}


String getDateTimeString() {
  time_t now;
  struct tm* timeinfo;
  char buffer[40];
  time(&now);
  timeinfo = localtime(&now);
  strftime(buffer, sizeof(buffer), "%A %Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}


void handleNewMessages(int numNewMessages) {
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    
    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
      String welcome = "Welcome, " + from_name + "!\n";
      welcome += "Use the following commands to interact with the ESP32-CAM\n";
      welcome += "/photo or /p or p or P: takes a new photo\n";
      welcome += "/flash or /f or f or F: toggles flash LED\n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/flash" || text == "/f" || text == "f" || text == "F") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      Serial.println("Change flash LED state");
    }
    if (text == "/photo" || text == "/p" || text == "p" || text == "P") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
    if (text[0] == 'b' || text[0] == 'B') {
      brightness_g = get_brightness(text, text[0]);
      //brightness_g = constrain(brightness, 0, 50);
      //analogWrite(FLASH_LED_PIN, brightness);
      Serial.print("Set flash brightness to: ");
      Serial.println(brightness_g);
      bot.sendMessage(CHAT_ID, "Set flash brightness to: " + String(brightness_g), "");
    }
    if (text[0] == 'i' || text[0] == 'I')  {
      brightness_g = get_brightness(text, text[0]);
      //brightness_g = constrain(brightness, 0, 50);
      //analogWrite(FLASH_LED_PIN, brightness);
      Serial.print("Set flash brightness to: ");
      Serial.println(brightness_g);
      //bot.sendMessage(CHAT_ID, "Flash Brightness: " + String(brightness_g), "");
      bot.sendMessage(CHAT_ID, "Snap - " + getDateTimeString());
      sendPhoto = true;
      Serial.println("New photo request with brightness: " + String(brightness_g));
    }

    // Testing various things
    if (text == "t" || text == "T") {
      Serial.println("Testing now...");
      bot.sendMessage(CHAT_ID, "Testing in progress...", "");
      for (int b = 0; b < 50; b++) {
        analogWrite(FLASH_LED_PIN, b);
        delay(1000);
        Serial.println(b);
      }
      analogWrite(FLASH_LED_PIN, 0);
    }
  }
}


String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t *fb = NULL;

  // TODO: Is that right? Dispose first picture because of bad quality
  if (false) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb); // dispose the buffered image
  }

  // Take a new photo
  fb = esp_camera_fb_get();  
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    String head = "--M0RVL\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--M0RVL\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--M0RVL--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=M0RVL");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n+1024 < fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length() == 0) state = true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length() > 0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "ERROR: Connection to api.telegram.org failed.";
    Serial.println("ERROR: Connection to api.telegram.org failed.");
  }
  return getBody;
}


void connect_to_wifi() {
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
  WiFi.begin(SSID, WIFI_PASSWORD);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP()); 
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  // Init Serial Monitor
  Serial.begin(115200);

  // Set LED Flash as output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  // Config and init the camera
  configInitCamera();

  // Connect to Wi-Fi
  connect_to_wifi();

  // TODO: Need to be connected to Wifi to get the MAC address. Not ok.
  const String MAC_2 = "0C:B8:15:F5:A6:2C";
  const String MAC = WiFi.macAddress();
  Serial.print("ESP32 MAC: ");
  Serial.println(MAC);

  if (MAC == MAC_2) {
    Serial.println("Using BOTtoken_2");
    BOTtoken = BOTtoken_2;
  } else {
    Serial.println("Using BOTtoken_1");
    BOTtoken = BOTtoken_1;
  }
  bot.updateToken(BOTtoken);

  // Initialize NTP and get the time
  setup_time();
}


void loop() {
  //Serial.println("Top of loop()");
  
  // Reconnect to WiFi if connection is lost
  static unsigned long previousMillis = 0;
  const unsigned long CHECK_WIFI_TIME_MSECS = 30000;
  unsigned long currentMillis = millis();
  // If WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= CHECK_WIFI_TIME_MSECS)) {
    connect_to_wifi();
    bot.sendMessage(CHAT_ID, "Reconnected to WiFi", "");
    previousMillis = currentMillis;
  }

  static int current_hour = -1;
  static int current_day = -1;
  struct tm* currentDateTime = getDateTime();
  if (currentDateTime->tm_hour == HOUR_TO_SEND_PHOTO &&
      currentDateTime->tm_yday != current_day) {
    Serial.println("======= Sending the daily photo =======");
    
    brightness_g = 10;  // TODO: make configurable
    sendPhoto = true;
    current_day = currentDateTime->tm_yday;

    bot.sendMessage(CHAT_ID, "Daily Snap - " + getDateTimeString());
  }
  
  if (sendPhoto) {
    // Turn on flash LED before taking a photo
    //digitalWrite(FLASH_LED_PIN, HIGH);
    analogWrite(FLASH_LED_PIN, brightness_g);

    Serial.println("Preparing photo");
    sendPhotoTelegram(); 
    sendPhoto = false;
    Serial.println("Photo sent");
    
    // Turn off flash LED after taking a photo
    //digitalWrite(FLASH_LED_PIN, LOW);
    analogWrite(FLASH_LED_PIN, 0);
  }
  
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // Loop delay
  //Serial.println("Sleeping 1 second...");
  delay(1000);
}
