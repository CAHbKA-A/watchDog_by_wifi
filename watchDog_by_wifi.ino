// ============================================================
// ESP8266 Watchdog: контроль интернета с веб-настройкой
// Версия: v1.2
// ------------------------------------------------------------
// ИСТОРИЯ ВЕРСИЙ:
// v1.2 - Время удержания реле при потере Wi-Fi увеличено до 5 секунд
//        (теперь совпадает с временем при потере интернета).
// v1.1 - Добавлен принудительный запуск портала при старте (3 минуты).
//        Профилактическая перезагрузка теперь привязана ко времени
//        (20 дней с момента включения), а не к количеству пингов.
// v1.0 - Базовая версия: WiFiManager, EEPROM, проверка ping, реле.
// ------------------------------------------------------------
// • При включении ВСЕГДА создаёт точку доступа "ESP_watchdog" на 3 минуты.
// • В это время можно зайти на 192.168.4.1 и изменить настройки.
//
// ⚠️ ВАЖНОЕ ПРИМЕЧАНИЕ ПО БРАУЗЕРАМ:
// Для входа на страницу настроек (192.168.4.1) с телефона 
// ИСПОЛЬЗУЙТЕ ЯНДЕКС БРАУЗЕР или FIREFOX. 
// Стандартные Chrome и Samsung Internet блокируют локальные 
// HTTP-запросы и могут выдавать ошибку "Сайт недоступен".
//
// • Если за 3 минуты пользователь ничего не сохранил, портал 
//   закрывается, и ESP подключается к ранее сохранённой сети Wi-Fi.
// • В рабочем режиме периодически проверяет доступность PING_HOST.
//   При FAIL_LIMIT неудачах подряд включает реле и перезагружается.
// • Профилактическая перезагрузка выполняется ровно через 20 дней
//   НЕПРЕРЫВНОЙ РАБОТЫ (с момента включения платы), независимо от 
//   того, были ли пинги успешными или нет.
// ============================================================

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <EEPROM.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // Встроенный светодиод на ESP8266 (обычно GPIO2)
#endif

// ---------- Карта EEPROM ----------
// В EEPROM храним только три пользовательских параметра.
// Флаг 0xAA в байте 48 означает, что данные уже записаны.
// SSID/пароль Wi-Fi хранит сам WiFiManager (в отдельной области Flash).
#define EEPROM_SIZE       512
#define ADDR_RESENDTIME   0    // unsigned long (4 байта) — период ping, мс
#define ADDR_FAIL_LIMIT   4    // int (4 байта)           — FAIL_LIMIT
#define ADDR_PING_HOST    8    // char[40]                — PING_HOST
#define ADDR_INIT_FLAG    48   // 1 байт: 0xAA = данные записаны

// ---------- Значения по умолчанию ----------
const unsigned long DEFAULT_RESENDTIME = 15UL * 60 * 1000; // 15 минут
const int DEFAULT_FAIL_LIMIT = 3;                         // 3 неудач подряд
const char* DEFAULT_PING_HOST = "8.8.8.8";                 // Google DNS

// ---------- Текущие настройки ----------
unsigned long resendtime = DEFAULT_RESENDTIME;  // период проверки, мс
int FAIL_LIMIT = DEFAULT_FAIL_LIMIT;            // порог неудач для реле
char pingHost[40] = "8.8.8.8";                  // что пингуем

// ---------- Пины и тайминги железа ----------
#define RELAY_PIN             4        // GPIO4 — управление реле
#define RESTART_DELAY         5000     // 5 секунд: время, на которое реле снимает питание
#define WIFI_CONNECT_TIMEOUT  180000   // 3 минуты — таймаут подключения/переподключения, мс
#define WIFI_CHECK_INTERVAL   500      // шаг опроса статуса Wi-Fi, мс

// ---------- Профилактическая перезагрузка ----------
// Считается от момента включения (bootTime). 
// 20 дней * 24 часа * 60 минут * 60 секунд * 1000 мс = 1 728 000 000 мс.
// Примечание: millis() переполняется через ~49.7 дней, поэтому 20 дней — безопасное значение.
#define PREVENTIVE_RESTART_DAYS 20
const unsigned long PREVENTIVE_RESTART_MS = PREVENTIVE_RESTART_DAYS * 24UL * 60UL * 60UL * 1000UL;

// ---------- Таймаут портала настройки ----------
#define CONFIG_PORTAL_TIMEOUT 180      // 3 минуты (180 секунд)

// ---------- Переменные состояния ----------
unsigned long now = 0;             // текущее millis()
unsigned long lastMeasure = 0;     // момент последней проверки ping
unsigned long bootTime = 0;        // момент старта (для профилактической перезагрузки)
int failCount = 0;                 // неудач подряд
int successCount = 0;              // успехов подряд (ТОЛЬКО для логов, не влияет на рестарт)
bool relayState = false;           // текущее состояние реле (для отладки)
bool shouldSaveConfig = false;     // выставляется в callback при нажатии "Save"

// ---------- Прототипы ----------
void loadConfig();
void saveConfig();
void saveConfigCallback();
void applyAndSave(WiFiManagerParameter &pResend, WiFiManagerParameter &pFail, WiFiManagerParameter &pHost);
bool checkInternet();
void restartESP();

// ============================================================
// Загрузка настроек из EEPROM
// ============================================================
void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(ADDR_INIT_FLAG) == 0xAA) {
    EEPROM.get(ADDR_RESENDTIME, resendtime);
    EEPROM.get(ADDR_FAIL_LIMIT, FAIL_LIMIT);
    for (int i = 0; i < 40; i++) {
      pingHost[i] = EEPROM.read(ADDR_PING_HOST + i);
    }
    pingHost[39] = '\0';   // гарантируем нуль-терминатор
  }
  EEPROM.end();

  Serial.println("Config loaded:");
  Serial.print("  resendtime (min): "); Serial.println(resendtime / 60000);
  Serial.print("  FAIL_LIMIT: ");       Serial.println(FAIL_LIMIT);
  Serial.print("  PING_HOST: ");        Serial.println(pingHost);
}

// ============================================================
// Сохранение настроек в EEPROM
// ============================================================
void saveConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(ADDR_RESENDTIME, resendtime);
  EEPROM.put(ADDR_FAIL_LIMIT, FAIL_LIMIT);
  for (int i = 0; i < 40; i++) {
    EEPROM.write(ADDR_PING_HOST + i, (i < strlen(pingHost)) ? pingHost[i] : 0);
  }
  EEPROM.write(ADDR_INIT_FLAG, 0xAA);
  EEPROM.commit();
  EEPROM.end();

  Serial.println("Config saved to EEPROM");
}

// ============================================================
// Callback WiFiManager: пользователь нажал "Save"
// ============================================================
void saveConfigCallback() {
  shouldSaveConfig = true;
}

// ============================================================
// Применить значения из полей веб-формы и сохранить в EEPROM
// ============================================================
void applyAndSave(WiFiManagerParameter &pResend, WiFiManagerParameter &pFail, WiFiManagerParameter &pHost) {
  unsigned long newResend = atol(pResend.getValue()) * 60000UL;   // мин → мс
  int newFail = atoi(pFail.getValue());
  const char* newHost = pHost.getValue();

  if (newResend > 0)  resendtime = newResend;
  if (newFail > 0)    FAIL_LIMIT = newFail;
  if (strlen(newHost) > 0) {
    strncpy(pingHost, newHost, 39);
    pingHost[39] = '\0';
  }

  saveConfig();

  Serial.println("Settings applied and saved:");
  Serial.print("  resendtime (min): "); Serial.println(resendtime / 60000);
  Serial.print("  FAIL_LIMIT: ");       Serial.println(FAIL_LIMIT);
  Serial.print("  PING_HOST: ");        Serial.println(pingHost);
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println();
  Serial.println("========================================");
  Serial.println("ESP8266 STARTUP (v1.2)");
  Serial.println("========================================");

  // Пины: LED и реле выключены (активный уровень реле — LOW)
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(RELAY_PIN, HIGH);

  // 1. Запоминаем момент старта для профилактической перезагрузки
  bootTime = millis();

  // 2. Читаем сохранённые настройки (или значения по умолчанию)
  loadConfig();

  // 3. Настраиваем WiFiManager
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);

  // Готовим строковые представления текущих значений для полей формы
  char resendtimeStr[16];
  snprintf(resendtimeStr, sizeof(resendtimeStr), "%lu", resendtime / 60000UL);
  char failLimitStr[8];
  snprintf(failLimitStr, sizeof(failLimitStr), "%d", FAIL_LIMIT);

  // Три пользовательских поля на веб-странице
  WiFiManagerParameter paramResend("resend_min", "Период отправки ping (мин)", resendtimeStr, 16);
  WiFiManagerParameter paramFail("fail_limit", "Количество неудачных ping для перезагрузки", failLimitStr, 8);
  WiFiManagerParameter paramHost("ping_host", "Что пингуем", pingHost, 40);

  wifiManager.addParameter(&paramResend);
  wifiManager.addParameter(&paramFail);
  wifiManager.addParameter(&paramHost);

  // 4. ПРИНУДИТЕЛЬНО запускаем точку доступа на 3 минуты при каждом включении
  Serial.println("Starting Config Portal. Connect to AP 'ESP_watchdog'...");
  bool connected = wifiManager.startConfigPortal("ESP_watchdog");

  // 5. Обработка результата после закрытия портала
  if (shouldSaveConfig) {
    // Пользователь нажал "Save" — применяем и сохраняем, затем перезагрузка
    Serial.println("User pressed Save. Applying settings...");
    applyAndSave(paramResend, paramFail, paramHost);
    Serial.println("Restarting to apply settings...");
    delay(1000);
    ESP.restart();
  } else if (!connected) {
    // Портал закрылся по таймауту 3 минут (пользователь ничего не нажал)
    Serial.println("Portal timeout. Attempting to connect to saved Wi-Fi...");
    
    String savedSSID = wifiManager.getWiFiSSID();
    String savedPass = wifiManager.getWiFiPass();

    if (savedSSID.length() > 0) {
      WiFi.begin(savedSSID.c_str(), savedPass.c_str());
      
      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to saved Wi-Fi!");
      } else {
        Serial.println("\nFailed to connect to saved Wi-Fi! Restarting...");
        ESP.restart();
      }
    } else {
      Serial.println("No saved Wi-Fi found. Restarting...");
      ESP.restart();
    }
  } else {
    // Подключились (если портал закрылся по другой причине)
    Serial.println("Connected to WiFi.");
  }

  // 6. Дальше — обычная работа
  Serial.println();
  Serial.println("Connected to Wifi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("PING_HOST: ");        Serial.println(pingHost);
  Serial.print("resendtime (min): "); Serial.println(resendtime / 60000);
  Serial.print("FAIL_LIMIT: ");       Serial.println(FAIL_LIMIT);
  Serial.print("Preventive restart in "); Serial.print(PREVENTIVE_RESTART_DAYS); Serial.println(" days.");

  // 7. Сброс таймера пинга
  lastMeasure = millis();
}

// ============================================================
// Проверка интернета
// ============================================================
bool checkInternet() {
  WiFiClient client;
  client.setTimeout(2000); // Меньше watchdog ESP8266 (~3 с)

  IPAddress ip;
  if (!ip.fromString(pingHost)) {
    // Если хост не IP — пробуем разрешить как доменное имя
    if (!WiFi.hostByName(pingHost, ip)) return false;
  }

  if (client.connect(ip, 53)) {
    client.stop();
    return true;
  }
  return false;
}

// ============================================================
// Перезагрузка ESP8266
// ============================================================
void restartESP() {
  Serial.println("========================================");
  Serial.println("!!! RESTARTING ESP8266 !!!");
  Serial.println("========================================");
  delay(500);
  ESP.restart();
}

// ============================================================
// Основной цикл
// ============================================================
void loop() {
  now = millis();

  // ---------- Проверка профилактической перезагрузки ----------
  // Выполняется ПЕРВОЙ, независимо от Wi-Fi и пингов.
  // Считается от bootTime (момента включения), не сбрасывается при сбоях.
  if (millis() - bootTime >= PREVENTIVE_RESTART_MS) {
    Serial.println("!!! PREVENTIVE RESTART (20 DAYS UPTIME) !!!");
    delay(1000);
    restartESP();
    return;
  }

  // ---------- Контроль Wi-Fi ----------
  // Если Wi-Fi отвалился — пытаемся переподключиться с таймаутом.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    unsigned long startReconnectTime = millis();
    WiFi.begin();   // использует сохранённые WiFiManager SSID/пароль

    while (WiFi.status() != WL_CONNECTED) {
      // Проверяем профилактический таймер даже внутри цикла ожидания Wi-Fi,
      // так как этот цикл блокирует основной loop().
      if (millis() - bootTime >= PREVENTIVE_RESTART_MS) {
        Serial.println("!!! PREVENTIVE RESTART (20 DAYS UPTIME) !!!");
        delay(1000);
        restartESP();
        return;
      }

      if (millis() - startReconnectTime > WIFI_CONNECT_TIMEOUT) {
        Serial.println("!!! WIFI RECONNECTION TIMEOUT !!!");
        
        // ВНИМАНИЕ: теперь импульс реле длится 5 секунд (RESTART_DELAY)
        digitalWrite(RELAY_PIN, LOW);
        relayState = true;
        delay(RESTART_DELAY); 
        
        ESP.restart();
        return;
      }
      delay(WIFI_CHECK_INTERVAL);
      Serial.print(".");
    }

    Serial.println();
    Serial.println("Reconnected");
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    failCount = 0;
    successCount = 0;
    lastMeasure = millis();   // сброс таймера, чтобы первый ping после
                              // переподключения был через resendtime
    return;
  }

  // ---------- Периодическая проверка интернета ----------
  if (now - lastMeasure > resendtime) {
    lastMeasure = now;   // отсчёт следующего интервала — с момента запуска проверки
    Serial.print("Checking internet... ");
    bool success = checkInternet();

    if (success) {
      // ----- Успех -----
      Serial.println("OK");
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
      digitalWrite(LED_BUILTIN, HIGH);

      failCount = 0;
      successCount++; // Счетчик успехов теперь нужен только для логов

      Serial.print("Success count: "); Serial.println(successCount);
    } else {
      // ----- Неудача -----
      Serial.println("Error!");
      digitalWrite(LED_BUILTIN, HIGH);

      failCount++;
      successCount = 0; // Обнуляем счетчик успехов (для логов)

      Serial.print("Fail count: "); Serial.print(failCount);
      Serial.print(" / ");          Serial.println(FAIL_LIMIT);

      // Порог неудач — включаем реле и перезагружаемся.
      // ВНИМАНИЕ: здесь импульс реле длится 5 секунд (RESTART_DELAY).
      if (failCount >= FAIL_LIMIT) {
        digitalWrite(RELAY_PIN, LOW);
        relayState = true;
        Serial.println("!!! RELAY TRIGGERED !!!");
        delay(RESTART_DELAY);
        restartESP();
      }
    }
    Serial.println("---");
  }
}
