/*
Adrian Rosoga
17 Nov 2025 - Initial commit after code cleanup

Based on "ESP32-CAM Telegram Photo Bot" tutorial from Random Nerd Tutorials
https://RandomNerdTutorials.com/telegram-esp32-cam-photo-arduino/
*/

#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>

#include "ntp_time.h"
#include "credentials.h"    // WIFI and Telegram credentials - sample below

// Debug logging control
#define ENABLE_DEBUG 1
#define LOG(...) do { if (ENABLE_DEBUG) { Serial.print(__VA_ARGS__); } } while(0)
#define LOGLN(...) do { if (ENABLE_DEBUG) { Serial.println(__VA_ARGS__); } } while(0)

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

#define FLASH_LED_PIN 4

// Main configs
const int HOUR_TO_SEND_PHOTO = 5; // 24-hour format
bool sendPhoto = false;
WiFiClientSecure clientTCP;
std::string BOTtoken = "DummyBotToken";             // Will be set in setup()
UniversalTelegramBot bot("", clientTCP); // token updated in setup()
int jpeg_quality = 10; // 10 was default, 0-63 lower number means higher quality
bool flashState = LOW;
int brightness_g = 255;   // Flash LED brightness (0-255) - This default is too bright

// Checks for new Telegram messages every 1 second.
const int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// CAMERA_MODEL_AI_THINKER
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


void configInitCamera() {
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

  // init with high specs to pre-allocate larger buffers
  if (psramFound()) {
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
  // Retry a few times before rebooting
  int tryCount = 0;
  while (err != ESP_OK && tryCount < 3) {
    Serial.printf("Camera init failed with error 0x%x, retrying (%d)\n", err, tryCount + 1);
    delay(1000);
    err = esp_camera_init(&config);
    tryCount++;
  }
  if (err != ESP_OK) {
    Serial.printf("Camera init failed after retries: 0x%x\n", err);
    // As a last resort restart
    delay(1000);
    ESP.restart();
  }

  // Take black and white pictures
  sensor_t *s = esp_camera_sensor_get();
  /* Set special effect to grayscale 
  0 - No Effect
  1 - Negative
  2 - Grayscale
  3 - Red Tint
  4 - Green Tint
  5 - Blue Tint
  6 - Sepia
  */
  s->set_special_effect(s, 2);
}


int get_brightness(const std::string &text, char delimiter=' ') {
  // Robust brightness parsing: accept forms like "b10", "b 10", "b=10"
  int brightness = -1;
  if (text.length() < 2) return brightness;
  // take everything after the first char
  std::string num = text.substr(1);
  // trim whitespace
  auto trim = [](std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) { s.clear(); return; }
    s = s.substr(start, end - start + 1);
  };
  trim(num);
  // support b=10 or b:10
  if (!num.empty() && (num[0] == '=' || num[0] == ':')) num = num.substr(1);
  trim(num);
  if (!num.empty()) {
    brightness = atoi(num.c_str());
  }
  return brightness;
}


std::string getDateTimeString() {
  time_t now;
  struct tm* timeinfo;
  char buffer[40];
  time(&now);
  timeinfo = localtime(&now);
  strftime(buffer, sizeof(buffer), "%A, %Y-%m-%d %H:%M:%S", timeinfo);
  return std::string(buffer);
}


void handleNewMessages(int numNewMessages) {
  Serial.print("Handle new messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    std::string chat_id = std::string(bot.messages[i].chat_id.c_str());
    if (chat_id != std::string(CHAT_ID)){
      bot.sendMessage(bot.messages[i].chat_id, "Unauthorized user");
      continue;
    }

    // Print the received message
    std::string text = std::string(bot.messages[i].text.c_str());
    Serial.println(bot.messages[i].text);

    std::string from_name = std::string(bot.messages[i].from_name.c_str());
    if (text == "/start") {
      char welcomeBuf[192];
      snprintf(welcomeBuf, sizeof(welcomeBuf), "Welcome, %s!\nUse the following commands to interact with the ESP32-CAM\n/photo or /p or p or P: takes a new photo\n/flash or /f or f or F: toggles flash LED\n", from_name.c_str());
      bot.sendMessage(CHAT_ID, welcomeBuf);
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
    if (!text.empty() && (text[0] == 'b' || text[0] == 'B')) {
      brightness_g = get_brightness(text);
      Serial.print("Set flash brightness to: ");
      Serial.println(brightness_g);
      char msgBuf[64];
      snprintf(msgBuf, sizeof(msgBuf), "Set flash brightness to: %d", brightness_g);
      bot.sendMessage(CHAT_ID, msgBuf);
    }
    if (!text.empty() && (text[0] == 'i' || text[0] == 'I'))  {
      brightness_g = get_brightness(text);
      Serial.print("Set flash brightness to: ");
      Serial.println(brightness_g);
      char snapBuf[128];
      snprintf(snapBuf, sizeof(snapBuf), "Snap - %s", getDateTimeString().c_str());
      bot.sendMessage(CHAT_ID, snapBuf);
      sendPhoto = true;
      Serial.print("New photo request with brightness: ");
      Serial.println(brightness_g);
    }

    // Testing various things
    if (text == "t" || text == "T") {
      Serial.println("Testing now...");
      bot.sendMessage(CHAT_ID, "Testing in progress...");
      for (int b = 0; b < 50; b++) {
        analogWrite(FLASH_LED_PIN, b);
        delay(1000);
        Serial.println(b);
      }
      analogWrite(FLASH_LED_PIN, 0);
    }
  }
}


std::string sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  // Response buffer to avoid Arduino String heap growth
  const size_t RESPONSE_BUF_SIZE = 2048;
  char responseBuf[RESPONSE_BUF_SIZE];
  size_t respLen = 0;
  responseBuf[0] = '\0';

  camera_fb_t *fb = NULL;

  // Dispose a possibly stale frame (improves first-frame quality)
  fb = esp_camera_fb_get();
  if (fb) {
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Take a new photo with a few retries
  int captureTries = 0;
  while (captureTries < 3) {
    fb = esp_camera_fb_get();
    if (fb) break;
    Serial.println("Camera capture failed, retrying...");
    delay(200);
    captureTries++;
  }
  if (!fb) {
    Serial.println("Camera capture failed after retries");
    return "Camera capture failed";
  }
  
  Serial.print("Connect to ");
  Serial.println(myDomain);

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    // Prepare multipart pieces (use const parts to avoid String concatenation)
    const char headPart1[] = "--M0RVL\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n";
    const char headPart2[] = "\r\n--M0RVL\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    const char tailArr[] = "\r\n--M0RVL--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = strlen(headPart1) + strlen(CHAT_ID) + strlen(headPart2) + strlen(tailArr);
    size_t totalLen = imageLen + extraLen;
    // Stream POST request line and headers without creating temporary Strings
    clientTCP.print("POST /bot");
    clientTCP.print(BOTtoken.c_str());
    clientTCP.println("/sendPhoto HTTP/1.1");
    clientTCP.print("Host: ");
    clientTCP.println(myDomain);
    char hdrBuf[64];
    snprintf(hdrBuf, sizeof(hdrBuf), "Content-Length: %u", (unsigned)totalLen);
    clientTCP.println(hdrBuf);
    clientTCP.println("Content-Type: multipart/form-data; boundary=M0RVL");
    clientTCP.println();

    // send multipart head
    clientTCP.print(headPart1);
    clientTCP.print(CHAT_ID);
    clientTCP.print(headPart2);
  
    // Send image in fixed-size chunks (avoid missing final chunk)
    size_t fbLen = fb->len;
    size_t sent = 0;
    while (sent < fbLen) {
      size_t chunk = (fbLen - sent) > 1024 ? 1024 : (fbLen - sent);
      clientTCP.write(fb->buf + sent, chunk);
      sent += chunk;
    }
    
    clientTCP.print(tailArr);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout waiting for HTTP response
    unsigned long startTimer = millis();
    // Read response in larger chunks into a preallocated buffer
    while (millis() - startTimer < (unsigned long)waitTime) {
      while (clientTCP.available()) {
        char buf[128];
        size_t len = clientTCP.readBytes(buf, sizeof(buf)-1);
        if (len > 0) {
          // copy bounded
          size_t canCopy = (RESPONSE_BUF_SIZE - 1 - respLen) < len ? (RESPONSE_BUF_SIZE - 1 - respLen) : len;
          if (canCopy > 0) {
            memcpy(responseBuf + respLen, buf, canCopy);
            respLen += canCopy;
            responseBuf[respLen] = '\0';
          }
          startTimer = millis();
        }
      }
      if (respLen > 0) break;
      delay(10);
    }
    clientTCP.stop();
    Serial.println(responseBuf);
  }
  else {
    const char* err = "ERROR: Connection to api.telegram.org failed.";
    Serial.println(err);
    return std::string(err);
  }
  return std::string(responseBuf);
}


void connect_to_wifi() {
  WiFi.mode(WIFI_STA);
  Serial.print("\nConnecting to WIFI ");
  Serial.println(SSID);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  WiFi.begin(SSID, WIFI_PASSWORD);

  // Exponential backoff reconnect attempts
  const int maxAttempts = 10;
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < maxAttempts) {
    unsigned long waitMs = 500UL * (1UL << min(attempt, 6)); // cap shift
    Serial.print(".");
    delay(waitMs);
    attempt++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Failed to connect to WiFi after attempts");
    // leave function and allow caller to decide (loop() will retry periodically)
    return;
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

  Serial.print("\nStarting up...");

  // Connect to Wi-Fi
  connect_to_wifi();

  // Basic runtime credentials check to avoid running without credentials
  if (strlen(CHAT_ID) == 0 || strlen(BOTtoken_1) < 10) {
    Serial.println("Missing credentials in credentials.h - please provide SSID, WIFI_PASSWORD, CHAT_ID and BOT tokens.");
    while (true) {
      delay(1000);
    }
  }

  // TODO: Need to be connected to Wifi to get the MAC address. Not ok.
  const char* MAC_2 = "0C:B8:15:F5:A6:2C";
  std::string MAC = std::string(WiFi.macAddress().c_str());
  Serial.print("ESP32 MAC: ");
  Serial.println(MAC.c_str());

  if (MAC == std::string(MAC_2)) {
    Serial.println("Using BOTtoken_2");
    BOTtoken = std::string(BOTtoken_2);
  } else {
    Serial.println("Using BOTtoken_1");
    BOTtoken = std::string(BOTtoken_1);
  }
  bot.updateToken(String(BOTtoken.c_str()));

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
    bot.sendMessage(CHAT_ID, "Reconnected to WiFi");
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

    char dailyBuf[128];
    snprintf(dailyBuf, sizeof(dailyBuf), "Daily Snap - %s", getDateTimeString().c_str());
    bot.sendMessage(CHAT_ID, dailyBuf);
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
