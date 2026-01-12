#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";        // Замените на имя вашей WiFi сети
const char* password = "YOUR_WIFI_PASSWORD"; // Замените на пароль вашей WiFi сети

// Создаем объекты
WebServer server(80);
Adafruit_AHTX0 aht;

// Переменные для хранения данных
float temperature = 0.0;
float humidity = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Инициализация I2C (по умолчанию SDA=21, SCL=22 на ESP32)
  Wire.begin();
  
  // Инициализация датчика AHT10
  Serial.println("Инициализация AHT10...");
  if (!aht.begin()) {
    Serial.println("Не удалось найти AHT10!");
    while (1) delay(10);
  }
  Serial.println("AHT10 найден!");
  
  // Подключение к WiFi
  Serial.print("Подключение к WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi подключен!");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());
  
  // Настройка маршрутов веб-сервера
  server.on("/", handleRoot);
  server.on("/data", handleData);
  
  // Запуск сервера
  server.begin();
  Serial.println("HTTP сервер запущен");
}

void loop() {
  server.handleClient();
}

// Главная страница с интерфейсом
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 + AHT10</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 500px;
            width: 100%;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 30px;
            font-size: 28px;
        }
        .sensor-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
            color: white;
            box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        }
        .sensor-label {
            font-size: 14px;
            opacity: 0.9;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .sensor-value {
            font-size: 42px;
            font-weight: bold;
            text-align: center;
        }
        .sensor-unit {
            font-size: 24px;
            opacity: 0.8;
        }
        .update-time {
            text-align: center;
            color: #666;
            font-size: 12px;
            margin-top: 20px;
        }
        .loading {
            text-align: center;
            color: #667eea;
            font-size: 16px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 Мониторинг AHT10</h1>
        
        <div class="sensor-card">
            <div class="sensor-label">🌡️ Температура</div>
            <div class="sensor-value">
                <span id="temperature">--</span>
                <span class="sensor-unit">°C</span>
            </div>
        </div>
        
        <div class="sensor-card" style="background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);">
            <div class="sensor-label">💧 Влажность</div>
            <div class="sensor-value">
                <span id="humidity">--</span>
                <span class="sensor-unit">%</span>
            </div>
        </div>
        
        <div class="update-time">
            Обновлено: <span id="updateTime">--</span>
        </div>
    </div>

    <script>
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temperature').textContent = data.temperature.toFixed(1);
                    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
                    
                    const now = new Date();
                    document.getElementById('updateTime').textContent = 
                        now.toLocaleTimeString('ru-RU');
                })
                .catch(error => {
                    console.error('Ошибка получения данных:', error);
                });
        }
        
        // Обновление данных каждые 2 секунды
        updateData();
        setInterval(updateData, 2000);
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

// API endpoint для получения данных в формате JSON
void handleData() {
  sensors_event_t humid, temp;
  
  // Чтение данных с датчика
  aht.getEvent(&humid, &temp);
  
  temperature = temp.temperature;
  humidity = humid.relative_humidity;
  
  // Формирование JSON ответа
  String json = "{";
  json += "\"temperature\":" + String(temperature, 2) + ",";
  json += "\"humidity\":" + String(humidity, 2);
  json += "}";
  
  server.send(200, "application/json", json);
}