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
#include <ArduinoOTA.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>

#include "ntp_time.h"
#include "credentials.h"    // WIFI and Telegram credentials - sample below
#include "version.h"        // Auto-generated: VERSION_FULL, VERSION_STRING, etc.

// Debug logging control
#define ENABLE_DEBUG 1
#define LOG(...) do { if (ENABLE_DEBUG) { Serial.print(__VA_ARGS__); } } while(0)
#define LOGLN(...) do { if (ENABLE_DEBUG) { Serial.println(__VA_ARGS__); } } while(0)

/*
Sample credentials file:

// WiFi credentials
const char* SSID = "XXX";
const char* WIFI_PASSWORD = "XXX";

// Use chat ID from @myidbot (C-string to avoid Arduino String heap usage)
const char* CHAT_ID = "XXX";

// Telegram Bot Tokens (C-strings)
const char* BOTtoken_1 = "XXX";
const char* BOTtoken_2 = "XXX";

// Optional OTA password (leave empty for no password). Set a short password for security.
const char* OTA_PASSWORD = "";
*/

std::string __version__ = VERSION_FULL;

#define FLASH_LED_PIN 4

// Main configs
const int HOUR_TO_SEND_PHOTO = 5; // 24-hour format
bool enablePhotoSending_g = false;
bool enableOneOffPhotoSending_g = false;

WiFiClientSecure clientTCP;
std::string BOTtoken = "ToBeUpdatedInSetup"; // Updated in setup()
UniversalTelegramBot bot("", clientTCP);

// JPEG quality range: 0-63 (lower = higher quality).
// Values below 4 cause capture failures at UXGA because the uncompressed
// output can exceed the DMA buffer the JPEG encoder allocates.
const int JPEG_QUALITY_MIN = 4;
const int JPEG_QUALITY_DEFAULT = 10;
int jpeg_quality_g = JPEG_QUALITY_DEFAULT;

bool flashState = LOW;
int brightness_g = 255;     // Flash LED brightness (0-255) - This default is too bright
int minutes_g = 60;         // Minutes interval for automatic photo sending, default is 5 minutes

// Checks for new Telegram messages every 1 second.
const int requestDelayInMilliseconds = 1 * 1000;
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
        config.jpeg_quality = jpeg_quality_g;  // 0-63 lower number means higher quality
        config.fb_count = 2;  // 2 buffers needed for CAMERA_GRAB_LATEST to work properly
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


int extract_parameter(const std::string &text, int minVal = 0, int maxVal = 255) {
    // Robust parameter parsing: accept forms like "b10", "b 10", "b=10"
    int value = -1;
    if (text.length() < 2) return value; // Not enough length for a command and parameter
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
        value = atoi(num.c_str());
        // Clamp to valid range
        if (value < minVal) value = minVal;
        if (value > maxVal) value = maxVal;
    }
    return value;
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
            char welcomeBuf[512];
            snprintf(welcomeBuf, sizeof(welcomeBuf),
                "Welcome, %s!\n"
                "Commands:\n"
                "  /photo /p p P - Take a photo\n"
                "  /flash /f f F - Toggle flash LED\n"
                "  b<N> - Set flash brightness (0-255)\n"
                "  m<N> - Set auto-snap interval (minutes)\n"
                "  i<N> - Snap with flash brightness N\n"
                "  q<N> - Set JPEG quality (0-63, lower=better)\n"
                "  w - Device info\n"
                "  r - Restart device\n",
                from_name.c_str());
            bot.sendMessage(CHAT_ID, welcomeBuf);
        }

        else if (text == "/flash" || text == "/f" || text == "f" || text == "F") {
            flashState = !flashState;
            digitalWrite(FLASH_LED_PIN, flashState);
            Serial.println("Change flash LED state");
        }

        else if (text == "/photo" || text == "/p" || text == "p" || text == "P") {
            enableOneOffPhotoSending_g = true;
            Serial.println("New snap request");
        }

        else if (!text.empty() && (text[0] == 'b' || text[0] == 'B')) {
            brightness_g = extract_parameter(text, 0, 255);
            Serial.print("Set flash brightness to: ");
            Serial.println(brightness_g);
            char msgBuf[64];
            snprintf(msgBuf, sizeof(msgBuf), "Set flash brightness to: %d", brightness_g);
            bot.sendMessage(CHAT_ID, msgBuf);
        }

        else if (!text.empty() && (text[0] == 'm' || text[0] == 'M')) {
            minutes_g = extract_parameter(text, 1, 1440);
            Serial.print("Set snap interval to ");
            Serial.print(minutes_g);
            Serial.println(" minute(s)");
            char msgBuf[64];
            snprintf(msgBuf, sizeof(msgBuf), "Set snap interval to %d minute(s)", minutes_g);
            bot.sendMessage(CHAT_ID, msgBuf);
        }

        else if (!text.empty() && (text[0] == 'r' || text[0] == 'R')) {
            Serial.println("ESP32 restarting on user command...");
            bot.sendMessage(CHAT_ID, "ESP32 restarting on user command...");
            // Drain pending messages so the restart command isn't re-processed on reboot
            delay(500);
            bot.getUpdates(bot.last_message_received + 1);
            delay(1000);
            ESP.restart();
        }

        else if (!text.empty() && (text[0] == 'w' || text[0] == 'W')) {
            Serial.println("Responding to who request...");
            who();
        }

        else if (!text.empty() && (text[0] == 'i' || text[0] == 'I'))  {
            brightness_g = extract_parameter(text, 0, 255);
            Serial.print("Set flash to ");
            Serial.println(brightness_g);
            
            char snapBuf[128];
            snprintf(snapBuf, sizeof(snapBuf), "Snap request (Flash %d) - %s", brightness_g, getDateTimeString().c_str());
            bot.sendMessage(CHAT_ID, snapBuf);

            enableOneOffPhotoSending_g = true;

            Serial.print("Snap request with flash ");
            Serial.println(brightness_g);
        }

        else if (!text.empty() && (text[0] == 'q' || text[0] == 'Q')) {
            jpeg_quality_g = extract_parameter(text, JPEG_QUALITY_MIN, 63);
            Serial.print("Set JPEG quality to: ");
            Serial.println(jpeg_quality_g);

            // Change quality at runtime via sensor API (no camera reinit needed)
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                s->set_quality(s, jpeg_quality_g);
            }
            
            char msgBuf[64];
            snprintf(msgBuf, sizeof(msgBuf), "Set JPEG quality to: %d", jpeg_quality_g);
            bot.sendMessage(CHAT_ID, msgBuf);
        }

        // Testing various things
        else if (text == "t" || text == "T") {
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


std::string sendPhotoTelegram(const char* photo_caption) {
    const char* myDomain = "api.telegram.org";
    // Response buffer to avoid Arduino String heap growth
    const size_t RESPONSE_BUF_SIZE = 2048;
    char responseBuf[RESPONSE_BUF_SIZE];
    size_t respLen = 0;
    responseBuf[0] = '\0';

    camera_fb_t *fb = NULL;

    // Discard stale buffered frame(s) before capturing the real photo.
    // Even with fb_count=2 and CAMERA_GRAB_LATEST, an explicit discard
    // ensures we get a frame captured *after* the flash LED is turned on.
    fb = esp_camera_fb_get();
    if (fb) {
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    delay(150);  // Let the sensor adjust to current lighting / flash

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
        return "Camera capture failed after retries";
    }
    
    Serial.print("Connect to ");
    Serial.println(myDomain);

    if (clientTCP.connect(myDomain, 443)) {
        Serial.println("Connection successful");
        
        // Prepare multipart pieces (use const parts to avoid String concatenation)
        const char headPart1[] = "--M0RVL\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n";
        const char captionPart[] = "\r\n--M0RVL\r\nContent-Disposition: form-data; name=\"caption\"; \r\n\r\n";
        const char headPart2[] = "\r\n--M0RVL\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
        const char tailArr[] = "\r\n--M0RVL--\r\n";

        size_t imageLen = fb->len;
        size_t extraLen = strlen(headPart1) + strlen(CHAT_ID) + strlen(captionPart) + strlen(photo_caption) + strlen(headPart2) + strlen(tailArr);
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
        clientTCP.print(captionPart);
        clientTCP.print(photo_caption);
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
        esp_camera_fb_return(fb);  // Avoid leaking the camera frame buffer
        const char* err = "ERROR: Connection to api.telegram.org failed.";
        Serial.println(err);
        return std::string(err);
    }
    
    // Check if Telegram API reported an error
    if (respLen > 0 && strstr(responseBuf, "\"ok\":false") != nullptr) {
        return "Telegram API returned an error";
    }
    return std::string("");
}


IPAddress connect_to_wifi() {
    WiFi.mode(WIFI_STA);
    Serial.print("\nConnecting to WIFI ");
    Serial.println(SSID);
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
        return IPAddress(0, 0, 0, 0);
    }
    Serial.print("\nESP32 IP: ");
    Serial.println(WiFi.localIP());

    return WiFi.localIP();
}


void who() {
    const std::string MAC = std::string(WiFi.macAddress().c_str());
    // Store toString() result to keep the buffer alive while we use c_str()
    String ip_str = WiFi.localIP().toString();
    const char* ip_address = ip_str.c_str();

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ESP32 v%s!\n\nOTA enabled!\n\nHostname=%s\nMAC=%s\nIP=%s\nJPEG Quality=%d\n\n%s",  __version__.c_str(), ArduinoOTA.getHostname().c_str(), MAC.c_str(), ip_address, jpeg_quality_g, getDateTimeString().c_str());
    bot.sendMessage(CHAT_ID, buffer);
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

    // Set root certificate for api.telegram.org (once, before any TLS connection)
    clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    Serial.print("\n\n=== ESP32 Starting up...");

    // Connect to Wi-Fi
    IPAddress ip_address = connect_to_wifi();

    // Initialize OTA updates
    {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        size_t startIdx = mac.length() > 6 ? mac.length() - 6 : 0;
        String host = "esp32cam-" + mac.substring(startIdx);
        ArduinoOTA.setHostname(host.c_str());
        if (strlen(OTA_PASSWORD) > 0) {
            ArduinoOTA.setPassword(OTA_PASSWORD);
        }
        ArduinoOTA.onStart([]() {
            Serial.println("Begin OTA");
        });
        ArduinoOTA.onEnd([]() {
            Serial.println("End OTA");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t err) {
            Serial.printf("OTA Error[%u]: ", err);
            if (err == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (err == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (err == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (err == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (err == OTA_END_ERROR) Serial.println("End Failed");
        });
        ArduinoOTA.begin();
        Serial.print("OTA ready. Hostname: ");
        Serial.println(host);
    }

    // Basic runtime credentials check to avoid running without credentials
    if (strlen(CHAT_ID) == 0 || strlen(BOTtoken_1) < 10) {
        Serial.println("Missing credentials in credentials.h - please provide SSID, WIFI_PASSWORD, CHAT_ID and BOT tokens.");
        while (true) {
            delay(1000);
        }
    }

    // TODO: Need to be connected to Wifi to get the MAC address. Not ok. Refactor to get MAC address without WiFi connection or move this block after WiFi connection.
    const char* MAC_1 = "0C:B8:15:F7:53:38";
    const char* MAC_2 = "0C:B8:15:F5:A6:2C";
    const std::string MAC = std::string(WiFi.macAddress().c_str());
    Serial.print("ESP32 MAC: ");
    Serial.println(MAC.c_str());

    if (MAC == std::string(MAC_2)) {

        Serial.println("Using BOTtoken_2");
        BOTtoken = std::string(BOTtoken_2);
        brightness_g = 40;  // TODO: Hardcoded value for boiler

    } else if (MAC == std::string(MAC_1)) {
        
        Serial.println("Using BOTtoken_1");
        BOTtoken = std::string(BOTtoken_1);
        brightness_g = 10;  // TODO: Hardcoded value for water softener

    } else {
        Serial.printf("ERROR: Unsupported ESP with MAC address %s", MAC.c_str());
    }
    bot.updateToken(String(BOTtoken.c_str()));

    // Initialize NTP and get the time
    setup_time();

    // Send startup message after 10 seconds delay
    delay(10000);
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ESP32 v%s\n\nOTA enabled!\n\nHostname=%s\nMAC=%s\nIP=%s\nJPEG Quality=%d\n\n%s",  __version__.c_str(), ArduinoOTA.getHostname().c_str(), MAC.c_str(), ip_address.toString().c_str(), jpeg_quality_g, getDateTimeString().c_str());
    bot.sendMessage(CHAT_ID, buffer);
}


void loop() {
    //Serial.println("Top of loop()");

    // Handle OTA events
    ArduinoOTA.handle();
    
    // Reconnect to WiFi if connection is lost
    static unsigned long previousMillis = 0;
    const unsigned long CHECK_WIFI_TIME_MSECS = 30 * 1000;
    unsigned long currentMillis = millis();
    // If WiFi is down, try reconnecting every CHECK_WIFI_TIME_MSECS milliseconds
    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= CHECK_WIFI_TIME_MSECS)) {
        IPAddress ip = connect_to_wifi();
        if (ip != IPAddress(0, 0, 0, 0)) {
            bot.sendMessage(CHAT_ID, "Reconnected to WiFi");
        }
        previousMillis = currentMillis;
    }

    static int photoSendCounter = 0;

    char photo_caption[128];
    if (strcmp(BOTtoken.c_str(), BOTtoken_1) == 0) {
        //
        // Daily photo at a specific hour of the day (e.g., 5 AM)
        // Water Softener
        //
        static int current_day = -1;
        struct tm* currentDateTime = getDateTime();
        if (currentDateTime->tm_hour == HOUR_TO_SEND_PHOTO &&
                currentDateTime->tm_yday != current_day) {
            Serial.println("======= Sending the daily photo =======");
            
            enablePhotoSending_g = true;
            current_day = currentDateTime->tm_yday;
                    
            photoSendCounter++;
            snprintf(photo_caption, sizeof(photo_caption), "Daily Snap (Flash %d) - %s (count %d)", brightness_g, getDateTimeString().c_str(), photoSendCounter);
            //bot.sendMessage(CHAT_ID, photo_caption);
        }
    } else if (strcmp(BOTtoken.c_str(), BOTtoken_2) == 0) {
        //
        // Testing: periodic photo every N minutes
        // Water pressure heating system 
        //
        static time_t old_secs = 0;
        time_t now;
        auto secs = time(&now);  // returns seconds since 1970-01-01

        if (secs - old_secs >= minutes_g * 60) {
            Serial.println("======= Sending photo =======");
            
            enablePhotoSending_g = true;

            photoSendCounter++;
            snprintf(photo_caption, sizeof(photo_caption), "Snap every %d minute(s) (Flash %d) - %s (count %d)", minutes_g, brightness_g, getDateTimeString().c_str(), photoSendCounter);
            //bot.sendMessage(CHAT_ID, photo_caption);

            old_secs = secs;
        }
    } else {
        Serial.printf("ERROR - unexpected BOTtoken value: %s\n", BOTtoken.c_str());
    }

    if (enableOneOffPhotoSending_g) {
        photoSendCounter++;
        snprintf(photo_caption, sizeof(photo_caption), "One-off Snap (Flash %d) - %s (count %d)", brightness_g, getDateTimeString().c_str(), photoSendCounter);
        //bot.sendMessage(CHAT_ID, photo_caption);
         Serial.println("One-off photo sending enabled");
    }
    
    if (enablePhotoSending_g || enableOneOffPhotoSending_g) {
        // Turn on flash LED before taking a photo
        analogWrite(FLASH_LED_PIN, brightness_g);

        Serial.println("Preparing photo...");
        std::string return_msg = sendPhotoTelegram(photo_caption);

        // Turn off flash LED after taking a photo
        analogWrite(FLASH_LED_PIN, 0);

        if (!return_msg.empty()) {
            bot.sendMessage(CHAT_ID, (std::string("ERROR: Taking and/or when sending photo: ") + return_msg).c_str());
            Serial.println((std::string("ERROR: Taking and/or when sending photo: ") + return_msg).c_str());
        } 
        
        enablePhotoSending_g = false;
        enableOneOffPhotoSending_g = false;
        Serial.println("After sending or attempting to take and send the photo");
    }

    // Restart is now handled directly in handleNewMessages after draining pending messages
    
    if (millis() > lastTimeBotRan + requestDelayInMilliseconds)  {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages) {
            Serial.println("Got response");
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastTimeBotRan = millis();
    }

    // Loop delay 1 second
    //Serial.println("Sleeping 1 second...");
    delay(1000);
}
