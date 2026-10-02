#include <ESP8266WiFi.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2  // Встроенный светодиод на ESP8266 (обычно GPIO2)
#endif

// Настройки WiFi
const char* ssid = "A";
const char* password = "s";

// Настройки проверки интернета
#define PING_HOST "8.8.8.8"     // Хост для проверки (Google DNS)

// Настройки реле
#define RELAY_PIN 4      // Пин для управления реле (GPIO4)
#define FAIL_LIMIT 10    // Количество неудачных проверок для сработки реле сработет через 2 часа после порпадания инетрнета
#define RESTART_DELAY 5000 // Задержка перед перезагрузкой (5 секунд)
#define FIRST_DELAY 3 // Задержка перед подключением к вайфай в минутах [ (1 минута)


// Настройки счётчика успешных пингов
#define SUCCESS_LIMIT 2400 // Количество успешных проверок для перезагрузки самой ESP. примерно ресарт ESP раз в 25 дней.

// Настройки подключения к WiFi
#define WIFI_CONNECT_TIMEOUT 180000 // 3 минуты (в миллисекундах). Время на успешное подключение. если не подключится, рестарт роутера
#define WIFI_CHECK_INTERVAL 500     // Интервал проверки подключения (500 мс)

// Таймеры
unsigned long now = 0;
unsigned long lastMeasure = 0;
const unsigned long resendtime = 15UL * 60 * 1000; // 15 минут между проверками

// Счетчики
int failCount = 0;        // Счетчик неудачных проверок
int successCount = 0;     // Счетчик успешных проверок
bool relayState = false;  // Текущее состояние реле (false - выключено, true - включено)

void setup() {
  Serial.begin(9600);
  delay(100);

  Serial.println();
  Serial.println("========================================");
  Serial.println("ESP8266 STARTUP");
  Serial.println("========================================");

  // Настройка пинов
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Выключаем светодиод
  digitalWrite(RELAY_PIN, HIGH);     // Реле выключено
    Serial.print("Waiting for ");
  Serial.print(FIRST_DELAY);
    Serial.println(" minutes");
  
  delay(FIRST_DELAY * 60000);
  setup_wifi();
}

void setup_wifi() {
  Serial.println();
  Serial.println("Starting..");
  delay(300);
  Serial.print("Connecting to Wifi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  int attempts = 0;

  // Пытаемся подключиться к WiFi в течение заданного времени
  while (WiFi.status() != WL_CONNECTED) {
    // Проверяем, не истекло ли время ожидания
    if (millis() - startAttemptTime > WIFI_CONNECT_TIMEOUT) {
      Serial.println();
      Serial.println("========================================");
      Serial.println("!!! WIFI CONNECTION TIMEOUT !!!");
      Serial.println("Cannot connect to WiFi within 3 minutes!");
      Serial.println("========================================");

      // Включаем реле
      digitalWrite(RELAY_PIN, LOW);
      relayState = true;
      Serial.println("!!! RELAY TRIGGERED !!!");

      delay(5000);

      // Перезагружаем плату
      Serial.println("Restarting ESP8266...");
      delay(500);
      ESP.restart();
      return; // Код дальше не выполнится, но для безопасности
    }

    delay(WIFI_CHECK_INTERVAL);
    Serial.print(".");
    attempts++;

    // Каждые 10 секунд выводим статус
    if (attempts % 20 == 0) {
      Serial.print(" [");
      Serial.print((millis() - startAttemptTime) / 1000);
      Serial.print("s] ");
    }
  }

  // Если подключились успешно
  Serial.println();
  Serial.println("Connected to Wifi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Connection time: ");
  Serial.print((millis() - startAttemptTime) / 1000);
  Serial.println(" seconds");
}

// Функция проверки интернета через соединение с DNS
bool checkInternet() {
  WiFiClient client;
  // Пытаемся подключиться к Google DNS (порт 53)
  if (client.connect(IPAddress(8, 8, 8, 8), 53)) {
    client.stop();
    return true;
  }
  return false;
}

// Функция перезагрузки ESP8266
void restartESP() {
  Serial.println("========================================");
  Serial.println("!!! RESTARTING ESP8266 !!!");
  Serial.println("========================================");
  delay(500);
  ESP.restart();
}

void loop() {
  now = millis();

  // Проверяем подключение к WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");

    // Пытаемся переподключиться с таймаутом
    unsigned long startReconnectTime = millis();
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      // Проверяем таймаут переподключения
      if (millis() - startReconnectTime > WIFI_CONNECT_TIMEOUT) {
        Serial.println();
        Serial.println("========================================");
        Serial.println("!!! WIFI RECONNECTION TIMEOUT !!!");
        Serial.println("Cannot reconnect to WiFi within 3 minutes!");
        Serial.println("========================================");

        // Включаем реле
        digitalWrite(RELAY_PIN, LOW);
        relayState = true;
        Serial.println("!!! RELAY TRIGGERED !!!");

        delay(1000);

        // Перезагружаем плату
        Serial.println("Restarting ESP8266...");
        delay(500);
        ESP.restart();
        return;
      }

      delay(WIFI_CHECK_INTERVAL);
      Serial.print(".");
    }

    // Если переподключились успешно
    Serial.println();
    Serial.println("Reconnected to Wifi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Сбрасываем счетчики при переподключении
    failCount = 0;
    successCount = 0;
    return;
  }

  // Основной цикл проверки интернета
  if (now - lastMeasure > resendtime) {
    lastMeasure = now;

    // Проверяем интернет
    Serial.print("Checking internet connection... ");
    bool success = checkInternet();

    if (success) {
      Serial.println("OK");

      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
      digitalWrite(LED_BUILTIN, HIGH);

      // УДАЧНАЯ ПРОВЕРКА - обнуляем счетчик неудач и увеличиваем счетчик успехов
      failCount = 0;
      successCount++;

      Serial.print("Fail count reset to: 0");
      Serial.print(" | Success count: ");
      Serial.print(successCount);
      Serial.print(" / ");
      Serial.println(SUCCESS_LIMIT);

      // Проверяем, не достигнут ли лимит успешных проверок
      if (successCount >= SUCCESS_LIMIT) {
        Serial.println("!!! SUCCESS LIMIT REACHED !!! Restarting ESP8266...");
        delay(1000);
        restartESP();
      }

    } else {
      Serial.println("Error!");
      digitalWrite(LED_BUILTIN, HIGH);

      // НЕУДАЧНАЯ ПРОВЕРКА - увеличиваем счетчик неудач и обнуляем счетчик успехов
      failCount++;
      successCount = 0;  // Обнуляем счетчик успехов при любой ошибке

      Serial.print("Fail count: ");
      Serial.print(failCount);
      Serial.print(" / ");
      Serial.print(FAIL_LIMIT);
      Serial.print(" | Success count reset to: 0");
      Serial.println();

      // Проверяем, не превышен ли лимит неудач
      if (failCount >= FAIL_LIMIT) {
        // Включаем реле
        digitalWrite(RELAY_PIN, LOW);
        relayState = true;
        Serial.println("!!! RELAY TRIGGERED !!! Fail limit exceeded!");

        Serial.print("Waiting ");
        Serial.print(RESTART_DELAY / 1000);
        Serial.println(" seconds before restart...");

        delay(RESTART_DELAY);
        restartESP();
      }
    }

    Serial.println("---");
  }
}
