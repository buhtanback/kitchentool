#include "Cofnig.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <HTTPClient.h>
#include <time.h>
#include <TimeLib.h> 

// Оголошення функцій
void connectToWiFi();
void updateTime();
void updateWeather();
void showWeather();
void showStopwatch();
void showWelcomeScreen();
float measureDistance();
bool isDistanceStable(float currentDistance);
void checkObjectDetection(unsigned long currentMillis);
void showError(String errorMessage);

// Налаштування дисплея
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

String weatherDescription;
float weatherTemp;

unsigned long previousMillis = 0;
const long interval = 900000;  // Інтервал оновлення погоди (15 хвилин)
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

const long timeUpdateInterval = 600000; // Інтервал оновлення часу (10 хвилин)
unsigned long lastTimeSyncMillis = 0;

const int DISTANCE_DISPLAY_THRESHOLD = 5; 

bool isLogoDisplayed = false;  // Змінна для відстеження стану логотипу
bool toggleLogo = false; 


WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 10800;  // UTC+3
NTPClient timeClient(ntpUDP, "time.nist.gov", utcOffsetInSeconds, 10000);  // Використовуйте новий офсет без додаткової компенсації

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    showImage(); // Виклик функції для відображення зображення
    connectToWiFi();
    timeClient.begin();  // Запуск NTPClient
    syncTime();
    updateWeather();
}

enum DisplayMode { WEATHER, STOPWATCH };
DisplayMode currentDisplayMode = WEATHER;

void loop() {
    unsigned long currentMillis = millis();

    // Автоматичне перепідключення до Wi-Fi при втраті з'єднання
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }

    // Оновлення часу раз на секунду
    if (!isLogoDisplayed && currentMillis - lastTimeUpdateMillis >= timeInterval) {
        lastTimeUpdateMillis = currentMillis;
        syncTime(); // Синхронізація часу
        if (currentDisplayMode == WEATHER && !isStopwatchActive) {
            showWeather();
        }
    }

    // Оновлення погоди кожні 15 хвилин
    if (!isLogoDisplayed && currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        updateWeather();
    }

    // Вимірювання відстані
    if (currentMillis - lastDistanceMeasureTime >= distanceMeasureInterval) {
        float distance = measureDistance();

        // Логіка для логотипу (відстань <= 5 см)
        if (distance <= 5 && (currentMillis - lastDebounceTime) > debounceDelay) {
            toggleLogo = !toggleLogo;
            if (toggleLogo) {
                showImage();  // Виводимо логотип
                isLogoDisplayed = true;
            } else {
                isLogoDisplayed = false;
                u8g2.clearBuffer();  // Очищаємо екран при вимкненні логотипу
                u8g2.sendBuffer();
            }
            lastDebounceTime = currentMillis; // Оновлюємо час останнього спрацювання
        }

        // Логіка для секундоміра (відстань <= 20 см, але більше 5 см)
        if (distance <= 20 && distance > 5 && (currentMillis - lastDebounceTime) > debounceDelay) {
            if (!isStopwatchActive) {
                isStopwatchActive = true;
                stopwatchStartTime = millis();
            } else {
                isStopwatchActive = false;
            }
            lastDebounceTime = currentMillis;
        }

        lastMeasuredDistance = distance;
        lastDistanceMeasureTime = currentMillis;
    }

    // Якщо логотип не відображається, оновлюємо інші режими
    if (!isLogoDisplayed) {
        if (isStopwatchActive) {
            showStopwatch();
        } else {
            showWeather();
        }
    }

    // Оновлення екрану секундоміра, якщо він запущений
    if (!isLogoDisplayed && isStopwatchActive) {
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
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Додано таймаут
    if (duration == 0) {
        Serial.println("Помилка вимірювання: немає відлуння");
        return -1; // Немає відлуння
    }
    float distance = (duration * 0.0343) / 2; // Відстань у см
    return distance;
}

// Функція для підключення до Wi-Fi з автоматичним перепідключенням
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
        showError("Немає Wi-Fi");
    }
}

void updateWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Attempting to update weather...");
        HTTPClient http;
        http.begin("http://api.openweathermap.org/data/2.5/weather?q=Khmelnytskyi&lang=ua&appid=89b5c4878e84804573dae7a6c3628e94&units=metric");
        int httpCode = http.GET();

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                Serial.println("Weather Payload: " + payload);
                DynamicJsonDocument doc(1024); // Зменшено розмір
                deserializeJson(doc, payload);
                weatherDescription = doc["weather"][0]["description"].as<String>();
                weatherTemp = doc["main"]["temp"];
                Serial.printf("Weather updated: %s, %.2f °C\n", weatherDescription.c_str(), weatherTemp);
            } else {
                Serial.printf("HTTP error: %d\n", httpCode);
            }
        } else {
            Serial.printf("HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
            showError("Помилка HTTP");
        }
        http.end();
    } else {
        Serial.println("Not connected to WiFi");
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

    // Встановлення більшого шрифту для часу та дати
    u8g2.setFont(u8g2_font_ncenB14_tr); // Зміна шрифту на більший

    // Отримання форматованого часу з NTP-клієнта
    String formattedTime = timeClient.getFormattedTime();

    // Виведення часу
    u8g2.setCursor(0, 40); // Координата Y для часу
    u8g2.print(formattedTime);

    // Отримання Unix-часу та конвертація в дату
    time_t rawTime = timeClient.getEpochTime();
    struct tm * timeInfo = localtime(&rawTime);

    // Форматування дати
    char dateStr[11]; // Рядок для дати
    sprintf(dateStr, "%02d-%02d-%04d", timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);

    // Виведення дати під часом
    u8g2.setCursor(0, 55); // Координата Y для дати
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

    if (millis() - lastSerialUpdateTime >= serialUpdateInterval && seconds != lastPrintedSecond) {
        Serial.printf("Stopwatch time: %02d:%02d\n", minutes, seconds);
        lastSerialUpdateTime = millis();
        lastPrintedSecond = seconds;
    }
}

void syncTime() {
    if (!timeClient.update()) {
        Serial.println("Failed to update time, retrying...");
        timeClient.forceUpdate(); 
    } else {
        Serial.println("Time updated successfully.");
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

void showImage() {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 16, 128, image); // Переконайтесь, що `image` визначено правильно
    u8g2.sendBuffer();
}
