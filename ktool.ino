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
void showFlappyBird(); // Нова функція для гри Flappy Bird
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

unsigned long stopwatchStartTime = 0;
unsigned long previousMillis = 0;
unsigned long lastObstacleUpdate = 0;
unsigned long lastPlayerUpdate = 0;
const long obstacleInterval = 2000;
const long playerFallInterval = 100;

int lastClkState; // Додаємо цю змінну для стану енкодера
int playerY = 32; // Початкова позиція гравця
int gravity = 1;  // Гравітація, яка тягне гравця вниз
int lift = -5;    // Сила підйому гравця при натисканні кнопки
bool buttonPressed = false;
int obstacleX = 128; // Початкова позиція перешкоди по осі X
int obstacleGap = 20; // Розмір проміжку в перешкодах

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
    
    // Перевірка обертання енкодера
    if (currentDisplayMode == MENU && currentClkState != lastClkState) {
        if (digitalRead(DT_PIN) != currentClkState) {
            // Обробка напрямку обертання для навігації по меню
            currentMenuItem = (MenuItem)((currentMenuItem + 1) % 3); // Рух до наступного пункту
        } else {
            currentMenuItem = (MenuItem)((currentMenuItem + 2) % 3); // Рух до попереднього пункту
        }
        lastClkState = currentClkState;
        showMenu(); // Оновлюємо відображення меню після зміни пункту
    }

    if (digitalRead(SW_PIN) == LOW && !buttonPressed) {
    buttonPressed = true;

    if (currentDisplayMode == MENU) {
        switch (currentMenuItem) {
            case WEATHER_ITEM:
                currentDisplayMode = WEATHER;
                inGame = false;  // Вихід із гри
                break;
            case STOPWATCH_ITEM:
                currentDisplayMode = STOPWATCH;
                inGame = false;
                break;
            case FLAPPY_BIRD_ITEM:
                currentDisplayMode = FLAPPY_BIRD;
                inGame = true;  // Вхід у гру
                playerY = 32;   // Скидаємо позицію для нової гри
                obstacleX = 128;
                break;
        }
        showCurrentScreen();
    } else if (currentDisplayMode == FLAPPY_BIRD && inGame) {
        // Якщо вже в грі, не виходимо в меню
        // Можна додати функціонал для керування гравцем у грі тут
    } else {
        currentDisplayMode = MENU;
        inGame = false;  // Вихід із гри
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

    // Оновлення позиції гравця
    if (currentMillis - lastPlayerUpdate >= playerFallInterval) {
        playerY += gravity;
        lastPlayerUpdate = currentMillis;
    }

    // Підйом гравця при натисканні кнопки
    if (digitalRead(SW_PIN) == LOW) {
        playerY += lift;
    }

    // Оновлення позиції перешкоди
    if (currentMillis - lastObstacleUpdate >= obstacleInterval) {
        obstacleX -= 2;
        if (obstacleX < -10) {
            obstacleX = 128;
        }
        lastObstacleUpdate = currentMillis;
    }

    // Перевірка зіткнень
    if ((obstacleX < 10 && (playerY < 25 || playerY > 45)) || playerY > 64 || playerY < 0) {
        playerY = 32;
        obstacleX = 128;
    }

    // Відображення гравця та перешкод
    u8g2.clearBuffer();
    u8g2.drawBox(5, playerY, 5, 5); // Гравець
    u8g2.drawBox(obstacleX, 0, 5, 25); // Верхня частина перешкоди
    u8g2.drawBox(obstacleX, 45, 5, 64); // Нижня частина перешкоди
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

void showStopwatch() {
    unsigned long elapsed = millis() - stopwatchStartTime;
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