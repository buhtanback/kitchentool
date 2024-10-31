#include "Cofnig.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <HTTPClient.h>
#include <time.h>
#include <TimeLib.h>

#define CLK_PIN 18
#define DT_PIN 19
#define SW_PIN 23

void connectToWiFi();
void updateTime();
void updateWeather();
void showWeather();
void showStopwatch();
void showFlappyBird();
void showMenu();
void showError(String errorMessage);
void syncTime();
void showCurrentScreen();

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 10800;  
NTPClient timeClient(ntpUDP, "time.nist.gov", utcOffsetInSeconds, 10000);

String weatherDescription;
float weatherTemp;

bool inGame = false;
bool stopwatchRunning = false;

unsigned long stopwatchStartTime = 0;
unsigned long previousMillis = 0;
unsigned long lastDrawTime = 0;
const long drawInterval = 50;

int lastClkState;
int playerY = 32;
int obstacleX = 128;
int gapY = 20;
const int gapHeight = 20;
int score = 0;
bool passedObstacle = false;
bool buttonPressed = false;

enum DisplayMode { MENU, WEATHER, STOPWATCH, FLAPPY_BIRD };
DisplayMode currentDisplayMode = MENU;

enum MenuItem { WEATHER_ITEM, STOPWATCH_ITEM, FLAPPY_BIRD_ITEM };
MenuItem currentMenuItem = WEATHER_ITEM;

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    u8g2.clearBuffer();
    connectToWiFi();
    timeClient.begin();
    syncTime();
    updateWeather();
    lastClkState = digitalRead(CLK_PIN);
    pinMode(CLK_PIN, INPUT);
    pinMode(DT_PIN, INPUT);
    pinMode(SW_PIN, INPUT_PULLUP);
    showMenu();
}

void loop() {
    unsigned long currentMillis = millis();

    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }

    int currentClkState = digitalRead(CLK_PIN);
    
    if (currentDisplayMode == MENU && currentClkState != lastClkState) {
        if (digitalRead(DT_PIN) != currentClkState) {
            currentMenuItem = (MenuItem)((currentMenuItem + 1) % 3);
        } else {
            currentMenuItem = (MenuItem)((currentMenuItem + 2) % 3);
        }
        lastClkState = currentClkState;
        showMenu();
    }

    if (digitalRead(SW_PIN) == LOW && !buttonPressed) {
        buttonPressed = true;

        if (currentDisplayMode == MENU) {
            switch (currentMenuItem) {
                case WEATHER_ITEM:
                    currentDisplayMode = WEATHER;
                    inGame = false;
                    break;
                case STOPWATCH_ITEM:
                    currentDisplayMode = STOPWATCH;
                    inGame = false;
                    stopwatchRunning = true;
                    stopwatchStartTime = millis();
                    break;
                case FLAPPY_BIRD_ITEM:
                    currentDisplayMode = FLAPPY_BIRD;
                    inGame = true;
                    playerY = 32;
                    obstacleX = 128;
                    score = 0;
                    passedObstacle = false;
                    break;
            }
            showCurrentScreen();
        } else if (currentDisplayMode == STOPWATCH) {
            if (stopwatchRunning) {
                stopwatchRunning = false;  // Зупиняємо секундомір
            } else {
                currentDisplayMode = MENU;  // Повернення в меню, якщо секундомір зупинений
                showMenu();
            }
        } else if (currentDisplayMode == FLAPPY_BIRD && inGame) {
            currentDisplayMode = MENU;
            inGame = false;
            showMenu();
        } else if (currentDisplayMode == WEATHER) {
            currentDisplayMode = MENU;
            showMenu();
        }
    } else if (digitalRead(SW_PIN) == HIGH) {
        buttonPressed = false;
    }

    if (currentDisplayMode == FLAPPY_BIRD) {
        showFlappyBird();
    } else if (currentDisplayMode == WEATHER && currentMillis - previousMillis >= 1000) {
        previousMillis = currentMillis;
        syncTime();
        showWeather();
    } else if (currentDisplayMode == STOPWATCH) {
        showStopwatch();
    }
}

void showFlappyBird() {
    unsigned long currentMillis = millis();

    int currentClkState = digitalRead(CLK_PIN);
    if (currentClkState != lastClkState) {
        if (digitalRead(DT_PIN) != currentClkState) {
            playerY = max(0, playerY - 1);
        } else {
            playerY = min(63, playerY + 1);
        }
        lastClkState = currentClkState;
    }

    if (currentMillis - lastDrawTime >= drawInterval) {
        lastDrawTime = currentMillis;

        obstacleX -= 2;

        if (obstacleX < -5) {
            obstacleX = 128;
            gapY = random(0, 64 - gapHeight);
            passedObstacle = false;
        }

        u8g2.clearBuffer();
        
        u8g2.drawBox(5, playerY, 5, 5);

        u8g2.drawBox(obstacleX, 0, 5, gapY); 
        u8g2.drawBox(obstacleX, gapY + gapHeight, 5, 64 - (gapY + gapHeight)); 

        if (obstacleX < 10 && (playerY < gapY || playerY > gapY + gapHeight)) {
            playerY = 32;
            obstacleX = 128;
            score = 0;
            passedObstacle = false;
        } else if (obstacleX < 5 && !passedObstacle) {
            score++;
            passedObstacle = true;
        }

        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 10);
        u8g2.print("Score: ");
        u8g2.print(score);

        u8g2.sendBuffer();
    }
}

void showStopwatch() {
    unsigned long elapsed = stopwatchRunning ? millis() - stopwatchStartTime : 0;
    int seconds = (elapsed / 1000) % 60;
    int minutes = (elapsed / 60000);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.print("Секундомір");

    u8g2.setFont(u8g2_font_ncenB24_tr);
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", minutes, seconds);

    uint8_t textWidth = u8g2.getStrWidth(timeStr);
    uint8_t textHeight = u8g2.getMaxCharHeight();

    uint8_t x = (128 - textWidth) / 2;
    uint8_t y = (64 - textHeight) / 2 + textHeight;

    u8g2.setCursor(x, y);
    u8g2.print(timeStr);
    u8g2.sendBuffer();
}

void showCurrentScreen() {
    u8g2.clearBuffer();
    if (currentDisplayMode == WEATHER) {
        showWeather();
    } else if (currentDisplayMode == STOPWATCH) {
        showStopwatch();
    } else if (currentDisplayMode == FLAPPY_BIRD) {
        showFlappyBird();
    }
}

void showMenu() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.print("Меню");

    u8g2.setCursor(0, 30);
    u8g2.print(currentMenuItem == WEATHER_ITEM ? "> Вулиця" : "  Вулиця");

    u8g2.setCursor(0, 45);
    u8g2.print(currentMenuItem == STOPWATCH_ITEM ? "> Секундомір" : "  Секундомір");

    u8g2.setCursor(0, 60);
    u8g2.print(currentMenuItem == FLAPPY_BIRD_ITEM ? "> Flappy Bird" : "  Flappy Bird");

    u8g2.sendBuffer();
}


void connectToWiFi() {
    Serial.println("Attempting to connect to WiFi...");
    WiFi.begin(ssid, password);
    int attempt = 0;
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        if (millis() - startAttemptTime >= 1000) {
            Serial.print(".");
            startAttemptTime = millis();
            attempt++;
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
    } else {
        Serial.println("Failed to connect to WiFi");
        showError("Немає Wi-Fi");
    }
}

void updateWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://api.openweathermap.org/data/2.5/weather?q=Khmelnytskyi&lang=ua&appid=89b5c4878e84804573dae7a6c3628e94&units=metric");
        int httpCode = http.GET();

        if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            weatherDescription = doc["weather"][0]["description"].as<String>();
            weatherTemp = doc["main"]["temp"];
        } else {
            showError("Помилка HTTP");
        }
        http.end();
    } else {
        showError("Немає Wi-Fi");
    }
}

void showWeather() {
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic); 

    String weatherInfo = weatherDescription + " " + String(weatherTemp, 2) + " C";
    u8g2.setCursor(0, 15);
    u8g2.print(weatherInfo);

    u8g2.setFont(u8g2_font_ncenB14_tr);
    String formattedTime = timeClient.getFormattedTime();
    u8g2.setCursor(0, 40);
    u8g2.print(formattedTime);

    time_t rawTime = timeClient.getEpochTime();
    struct tm * timeInfo = localtime(&rawTime);
    char dateStr[11];
    sprintf(dateStr, "%02d-%02d-%04d", timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);

    u8g2.setCursor(0, 55);
    u8g2.print(dateStr);
    u8g2.sendBuffer();
}



void showLogo() {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 16, 128, image);  // Використовуйте вашу функцію для зображення
    u8g2.sendBuffer();
}

void syncTime() {
    if (!timeClient.update()) {
        timeClient.forceUpdate(); 
    } else {
        setTime(timeClient.getEpochTime());
    }
}

void showError(String errorMessage) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 30);
    u8g2.print("Помилка:");
    u8g2.setCursor(0, 50);
    u8g2.print(errorMessage);
    u8g2.sendBuffer();
}