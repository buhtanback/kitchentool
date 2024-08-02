#include "Cofnig.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>

// Оголошення функцій
void connectToWiFi();
void updateTime();
void updateWeather();
void updateCurrentTime();
void showWeather();
void showStopwatch();
void showWelcomeScreen(); // Оголошення нової функції
float measureDistance();
bool isDistanceStable(float currentDistance);
void checkObjectDetection(unsigned long currentMillis);

// Налаштування дисплея
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

String currentDate;
String currentTime;
int currentHour, currentMinute, currentSecond;
unsigned long lastMillis;

String weatherDescription;
float weatherTemp;

unsigned long previousMillis = 0;
const long interval = 10000;  // Інтервал оновлення погоди (10 секунд)
const long timeInterval = 1000; // Інтервал оновлення часу (1 секунда)
unsigned long lastTimeUpdateMillis = 0;

const unsigned long distanceMeasureInterval = 100; // Інтервал вимірювання відстані (мс)
unsigned long lastDistanceMeasureTime = 0;
float lastMeasuredDistance = -1;
bool lastSensorActive = false;
bool isStopwatchActive = false;
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchElapsedTime = 0;
const float DISTANCE_STABILITY_THRESHOLD = 2.0; // Різниця в см між двома вимірюваннями

// Змінні для серійного монітора
unsigned long lastSerialUpdateTime = 0;
unsigned long serialUpdateInterval = 1000; // Інтервал оновлення серійного монітора (1 секунда)
int lastPrintedSecond = -1;

// Визначення пінів для датчика
#define TRIGGER_PIN 14
#define ECHO_PIN 27

const int DISTANCE_THRESHOLD = 20; // Поріг відстані в см
const unsigned long debounceDelay = 500; // Затримка для уникнення багатократного спрацьовування (мс)

unsigned long lastDebounceTime = 0; // Час останнього спрацьовування

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    showImage(); // Тепер викликаємо функцію для відображення зображення
    connectToWiFi();
    updateTime();
    updateWeather();
}

enum DisplayMode { WEATHER, STOPWATCH };
DisplayMode currentDisplayMode = WEATHER;

void loop() {
    unsigned long currentMillis = millis();

    // Оновлення часу кожну секунду
    if (currentMillis - lastTimeUpdateMillis >= timeInterval) {
        lastTimeUpdateMillis = currentMillis;
        updateCurrentTime();
        if (currentDisplayMode == WEATHER && !isStopwatchActive) {
            showWeather();
        }
    }

    // Оновлення погоди кожні 10 секунд
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        updateWeather();
    }

    // Вимірювання відстані
    if (currentMillis - lastDistanceMeasureTime >= distanceMeasureInterval) {
        float distance = measureDistance();
        bool sensorActive = (distance <= DISTANCE_THRESHOLD);

        if (isDistanceStable(distance)) {
            if (sensorActive && !lastSensorActive) {
                if (!isStopwatchActive) {
                    isStopwatchActive = true;
                    stopwatchStartTime = millis();
                    currentDisplayMode = STOPWATCH;
                    Serial.println("Stopwatch started.");
                } else {
                    isStopwatchActive = false;
                    stopwatchElapsedTime = millis() - stopwatchStartTime;
                    currentDisplayMode = WEATHER;
                    Serial.println("Stopwatch stopped.");
                }
            }
            lastSensorActive = sensorActive;
        }
        lastMeasuredDistance = distance;
        lastDistanceMeasureTime = currentMillis;
    }

    // Оновлення екрану секундоміра, якщо він запущений
    if (isStopwatchActive) {
        showStopwatch();
    }
}

bool isDistanceStable(float currentDistance) {
    if (lastMeasuredDistance < 0) return true; // Перше вимірювання
    return abs(currentDistance - lastMeasuredDistance) < DISTANCE_STABILITY_THRESHOLD;
}

// Функція для вимірювання відстані
float measureDistance() {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = (duration * 0.0343) / 2; // Відстань у см
    return distance;
}

// Функція для перевірки наявності об'єкта
void checkObjectDetection(unsigned long currentMillis) {
    float distance = measureDistance();
    if (distance <= DISTANCE_THRESHOLD && (currentMillis - lastDebounceTime) > debounceDelay) {
        if (!isStopwatchActive) { // Зміна з stopwatchRunning на isStopwatchActive
            isStopwatchActive = true;
            stopwatchStartTime = millis();
        } else {
            isStopwatchActive = false;
        }

        // Оновлення часу останнього спрацьовування
        lastDebounceTime = currentMillis;
    }
}

void showStopwatch() {
    unsigned long elapsed = millis() - stopwatchStartTime;
    int seconds = (elapsed / 1000) % 60;
    int minutes = (elapsed / 60000);

    u8g2.clearBuffer();

    // Встановіть шрифт для заголовка "Секундомір"
    u8g2.setFont(u8g2_font_cu12_t_cyrillic); // Використовується стандартний шрифт для заголовка
    u8g2.setCursor(0, 15);
    u8g2.print("Секундомір");

    // Встановіть більший шрифт для секунд і хвилин
    u8g2.setFont(u8g2_font_ncenB24_tr); // Використовується більший шрифт для цифр
    String timeStr = String(minutes) + ":" + String(seconds);
    
    // Визначте розміри тексту
    uint8_t textWidth = u8g2.getStrWidth(timeStr.c_str());
    uint8_t textHeight = u8g2.getMaxCharHeight();

    // Розрахуйте координати для центрування тексту
    uint8_t x = (128 - textWidth) / 2; // 128 - ширина екрану
    uint8_t y = (64 - textHeight) / 2 + textHeight; // 64 - висота екрану

    u8g2.setCursor(x, y);
    u8g2.print(timeStr);

    u8g2.sendBuffer();

    // Оновлення серійного монітора
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval && seconds != lastPrintedSecond) {
        Serial.printf("Stopwatch time: %02d:%02d\n", minutes, seconds);
        lastSerialUpdateTime = millis();
        lastPrintedSecond = seconds;
    }
}

void connectToWiFi() {
    Serial.println("Attempting to connect to WiFi...");
    WiFi.begin(ssid, password);
    int attempt = 0;
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && attempt < 30) { // Максимум 30 спроб
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
    }
}

void updateTime() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Attempting to update time...");
        HTTPClient http;
        http.begin("http://worldtimeapi.org/api/timezone/Europe/Kiev");
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("Payload: " + payload);  // Додано для налагодження
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            String dateTime = doc["datetime"];
            currentDate = dateTime.substring(2, 4) + "-" + dateTime.substring(5, 7) + "-" + dateTime.substring(8, 10); // Витягнути дату в потрібному форматі
            currentTime = dateTime.substring(11, 19); // Витягнути час з datetime
            currentHour = currentTime.substring(0, 2).toInt();
            currentMinute = currentTime.substring(3, 5).toInt();
            currentSecond = currentTime.substring(6, 8).toInt();
            lastMillis = millis();
            Serial.println("Time updated: " + currentTime);
            Serial.println("Date updated: " + currentDate);
        } else {
            Serial.println("HTTP GET failed: " + String(httpCode));
        }
        http.end();
    } else {
        Serial.println("Not connected to WiFi");
    }
}

void updateCurrentTime() {
    currentSecond++;
    if (currentSecond >= 60) {
        currentSecond = 0;
        currentMinute++;
        if (currentMinute >= 60) {
            currentMinute = 0;
            currentHour++;
            if (currentHour >= 24) {
                currentHour = 0;
            }
        }
    }
}

void updateWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Attempting to update weather...");
        HTTPClient http;
        http.begin("http://api.openweathermap.org/data/2.5/weather?q=Khmelnytskyi&lang=ua&appid=89b5c4878e84804573dae7a6c3628e94&units=metric");
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("Weather Payload: " + payload);  // Додано для налагодження
            DynamicJsonDocument doc(2048);
            deserializeJson(doc, payload);
            weatherDescription = doc["weather"][0]["description"].as<String>();
            weatherTemp = doc["main"]["temp"];
            Serial.printf("Weather updated: %s, %.2f °C\n", weatherDescription.c_str(), weatherTemp);
        } else {
            Serial.println("HTTP GET failed: " + String(httpCode));
        }
        http.end();
    } else {
        Serial.println("Not connected to WiFi");
    }
}

void showWeather() {
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic); 
    
    String weatherInfo = String(weatherDescription) + " " + String(weatherTemp, 2) + " C";
    u8g2.setCursor(0, 15);
    u8g2.print(weatherInfo);

    u8g2.setFont(u8g2_font_lubB18_tr); 
    
    // Створення строки для часу
    String timeInfo = "\n" + String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond);
    
    // Виведення часу
    u8g2.setCursor(0, 50);
    u8g2.print(timeInfo);

    u8g2.sendBuffer();  
}

void showImage() {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 16, 128, image); // Переконайтесь, що `image` визначено правильно
    u8g2.sendBuffer();
}