📖 Инструкция по эксплуатации Wi-Fi Watchdog (v1.2)
Железка следит за наличием интернета (IP можно указать любой). Если интернет пропадает (или указанный ip недоступен), она разрывает цепь питания вашего роутера на несколько секунд, вызывая его аппаратную перезагрузку. Вместо роутера запитать можно любую другую железку.

🛠 1. Аппаратная часть

Питание (IN 5..20V).  

Питание ESP и реле (3.3V): DC-DC преобразователь понижает напряжение до 3.3V для питания ESP-12F и модуля реле.

Управление реле: Пин GPIO4 на ESP-12F соединяется с пином IN на модуле реле.

Как это работает физически: Пока на пине GPIO4 нет минуса, реле выключено. Контакты COM и NC замкнуты, роутер работает. Когда ESP8266 подает низкий уровень (LOW), реле срабатывает, контакты COM и NC размыкаются, и роутер обесточивается.
При использовании другого реле, уровни надо будет поправить. У некоторых реле есть пермычки на уровень срабатывания.

⏱️ 2. Важное уточнение по времени отключения питания

Пропал интернет, но Wi-Fi сеть есть (роутер завис):
Питание с роутера снимается на 5 секунд.

Пропал сам Wi-Fi (ESP не видит сеть в течение 3 минут):
Питание снимается на 5 секунду.


🚀 3. Первое включение и настройка
Подайте питание на схему.

Плата создает открытую Wi-Fi сеть: ESP_watchdog (пароля нет).

Подключитесь к ней с телефона или ноутбука (мобильный интернет на телефоне лучше выключить).

Откройте Яндекс Браузер или Firefox (Chrome и Samsung Internet могут блокировать локальные страницы).

Введите адрес: http://192.168.4.1

Заполните поля:

Wi-Fi network: Выберите вашу домашнюю сеть.

Password: Пароль от вашего Wi-Fi.

Период отправки ping (мин): Как часто проверять интернет (по умолчанию каждые 15 минут).

Количество неудачных ping для перезагрузки: Сколько раз подряд должен не вернуться пинг для сработки реле (по умолчанию 3 раза подряд, это примерно 45 минут). При каждом удачном пинге счетчик fail обнуляется.

Что пингуем: Адрес для проверки (по умолчанию 8.8.8.8).

Нажмите Save. Устройство перезагрузится и начнет работу.

Если вы не нажали «Save» в течение 3 минут, портал закроется, и плата попытается подключиться к ранее сохраненной сети. Если сети нет — она перезагрузится и снова включит ESP_watchdog.

⚙️ 4. Рабочий режим
После подключения к вашему роутеру, плата работает автономно:

Интернет есть: Плата коротко мигает встроенным светодиодом каждую проверку.

Интернет пропал (несколько не вернувшихся пинга подряд)  — снимает питание с роутера на 5 секунд и перезагружается сама. После перезагрузки ESP снова подает питание на роутер.

Профилактика: Ровно через 20 дней непрерывной работы плата перезагрузится сама (не разрывая питание роутера), чтобы избежать зависаний и всяких переполнений.

🔄 5. Как изменить настройки позже?
Чтобы снова зайти в настройки, просто выключите и включите питание всей схемы. У вас будет 3 минуты, чтобы подключиться к сети ESP_watchdog, зайти на 192.168.4.1 и изменить параметры. Если ничего не трогать — она сама уйдет в рабочий режим.

📡 6. (Это в теории сам не пробовал). Обновление прошивки по Wi-Fi (OTA)
В веб-интерфейсе (по кнопке Update) можно залить новый код без проводов:

В Arduino IDE выберите Скетч → Экспорт бинарного файла (появится .bin файл).

Зайдите в настройки платы (через ESP_watchdog), нажмите Update, выберите .bin и дождитесь окончания (обычно 15–30 секунд).

Плата перезагрузится с новым кодом. Во время обновления не выключайте питание (риск повредить прошивку).нажмите Update, выберите .bin и дождитесь окончания (обычно 15–30 секунд).

Плата перезагрузится с новым кодом. Во время обновления не выключайте питание (риск повредить прошивку).

📖 Wi-Fi Watchdog User Manual (v1.2)

The device monitors internet connectivity (you can specify any IP address). If the internet goes down (or the specified IP becomes unreachable), it cuts the power to your router for a few seconds, forcing a hardware reboot. Instead of a router, you can power any other device through it.

🛠 1. Hardware

Power Supply (IN 5..20V).

ESP and Relay Power (3.3V): A DC-DC converter steps down the voltage to 3.3V to power the ESP-12F and the relay module.

Relay Control: The GPIO4 pin on the ESP-12F connects to the IN pin on the relay module.

How it works physically: As long as there is no ground (LOW) on the GPIO4 pin, the relay is off. The COM and NC contacts are closed, and the router is powered. When the ESP8266 outputs a LOW level, the relay triggers, the COM and NC contacts open, and the router loses power.

Note: If you use a different relay, you may need to adjust the logic levels. Some relays have jumpers to set the trigger level.

⏱️ 2. Important Clarification on Power Off Duration

Internet lost, but Wi-Fi network is present (router is frozen): Power is cut to the router for 5 seconds.

Wi-Fi itself is lost (ESP cannot see the network for 3 minutes): Power is cut for 5 seconds.

🚀 3. First Boot and Setup

Apply power to the circuit.

The board creates an open Wi-Fi network: ESP_watchdog (no password).

Connect to it from a phone or laptop (it is better to turn off mobile data on your phone).

Open Yandex Browser or Firefox (Chrome and Samsung Internet may block local pages).

Enter the address: http://192.168.4.1

Fill in the fields:

Wi-Fi network: Select your home network.

Password: Your Wi-Fi password.

Ping interval (min): How often to check the internet (default is every 15 minutes).

Number of failed pings to reboot: How many times in a row ping must fail to trigger the relay (default is 3 times in a row, which is about 45 minutes). The fail counter resets with every successful ping.

Ping target: The address to check (default is 8.8.8.8).

Click Save. The device will reboot and start working.

If you do not press "Save" within 3 minutes, the portal will close, and the board will try to connect to the previously saved network. If the network is unavailable, it will reboot and enable ESP_watchdog again.

⚙️ 4. Operating Mode
After connecting to your router, the board operates autonomously:

Internet is available: The board briefly blinks its built-in LED on every check.

Internet lost (several failed pings in a row): Cuts power to the router for 5 seconds and reboots itself. After rebooting, the ESP restores power to the router.

Preventive maintenance: Exactly after 20 days of continuous operation, the board reboots itself (without cutting power to the router) to prevent freezes and memory overflows.

🔄 5. How to change settings later?
To access the settings again, simply turn the power to the entire circuit off and on. You will have 3 minutes to connect to the ESP_watchdog network, go to 192.168.4.1, and change the parameters. If left untouched, it will automatically return to operating mode.

📡 6. Over-the-Air (OTA) Firmware Update (Theoretically, haven't tried it myself)
In the web interface (via the Update button), you can flash new code without wires:

In the Arduino IDE, select Sketch → Export compiled binary (a .bin file will appear).

Go to the board's settings (via ESP_watchdog), click Update, select the .bin file, and wait for it to finish (usually 15–30 seconds).

The board will reboot with the new code. Do not cut power during the update (risk of corrupting the firmware).