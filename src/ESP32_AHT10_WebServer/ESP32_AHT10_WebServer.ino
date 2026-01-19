#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <esp_system.h>

// Настройки WiFi
const char* ssid = "Network";  // update your network name
const char* password = "Password";  // updated the password for your network

WebServer server(80);
Adafruit_AHTX0 aht;

// Sensor's data
float temperature = 0.0;
float humidity = 0.0;
float minTemp = 999.0;
float maxTemp = -999.0;
float minHumid = 999.0;
float maxHumid = -999.0;

// История данных (последние 60 точек = 3 минуты при интервале 3 сек)
const int historySize = 60;
float tempHistory[historySize];
float humidHistory[historySize];
int historyIndex = 0;

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastWiFiCheck = 0;
unsigned long bootTime = 0;
const unsigned long sensorInterval = 3000;
const unsigned long wifiCheckInterval = 10000;

// Stat
int wifiReconnects = 0;

// Calculate CPU
unsigned long lastCpuCheck = 0;
unsigned long idleTime = 0;
unsigned long busyTime = 0;
float cpuUsage = 0.0;

// Объявления функций
void connectToWiFi();
void checkWiFiConnection();
void readSensor();
float calculateDewPoint(float temp, float humid);
float calculateHeatIndex(float temp, float humid);
void handleRoot();
void handleData();
void handleStats();
void handleHistory();
void handleReset();
void handleReboot();

void setup() {
  Serial.begin(115200);
  delay(1000);
  bootTime = millis();
  
  Serial.println("\n\n=== ESP32 Super Mini + AHT10 v2.0 ===");
  
  // Инициализация истории
  for(int i = 0; i < historySize; i++) {
    tempHistory[i] = 0;
    humidHistory[i] = 0;
  }
  
  // Инициализация I2C
  Wire.begin(8, 9);
  Wire.setClock(100000);
  delay(100);
  
  // Инициализация датчика AHT10
  Serial.println("Инициализация AHT10...");
  if (!aht.begin()) {
    Serial.println("ОШИБКА: AHT10 не найден!");
    while (1) delay(1000);
  }
  Serial.println("✓ AHT10 инициализирован");
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup for the routs
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/stats", HTTP_GET, handleStats);
  server.on("/history", HTTP_GET, handleHistory);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
  
  server.begin();
  Serial.println("✓ HTTP сервер запущен\n");
}

void loop() {
  unsigned long loopStart = micros(); // Launch of the cycle
  unsigned long currentMillis = millis();
  
  // Check WiFi
  if (currentMillis - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = currentMillis;
    checkWiFiConnection();
  }
  
  server.handleClient();
  
  // Read the sensor
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    readSensor();
  }
  
  unsigned long loopEnd = micros(); // Finish the cycle
  busyTime += (loopEnd - loopStart); // Life time
  
  delay(2);
  idleTime += 2000; // 2ms
  
  // Calculate the CPU every second
  if (currentMillis - lastCpuCheck >= 1000) {
    unsigned long totalTime = busyTime + idleTime;
    if (totalTime > 0) {
      cpuUsage = (float)busyTime / totalTime * 100.0;
    }
    // Sensor's reset
    busyTime = 0;
    idleTime = 0;
    lastCpuCheck = currentMillis;
  }
}

void connectToWiFi() {
  Serial.println("\n=== Подключение к WiFi ===");
  WiFi.disconnect(true);
  delay(1000);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  
  Serial.print("Подключение");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi подключен!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n✗ Ошибка подключения к WiFi");
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi потерян! Переподключение...");
    wifiReconnects++;
    connectToWiFi();
  }
}

void readSensor() {
  sensors_event_t humid, temp;
  
  if (aht.getEvent(&humid, &temp)) {
    temperature = temp.temperature;
    humidity = humid.relative_humidity;
    
    if (!isnan(temperature) && !isnan(humidity)) {
      // Update min/max
      if (temperature < minTemp) minTemp = temperature;
      if (temperature > maxTemp) maxTemp = temperature;
      if (humidity < minHumid) minHumid = humidity;
      if (humidity > maxHumid) maxHumid = humidity;
      
      // Adding to the history
      tempHistory[historyIndex] = temperature;
      humidHistory[historyIndex] = humidity;
      historyIndex = (historyIndex + 1) % historySize;
      
      Serial.printf("T: %.1f°C | H: %.1f%% | WiFi: %s\n", 
                    temperature, humidity, 
                    WiFi.status() == WL_CONNECTED ? "OK" : "ERR");
    }
  }
}

// Вычисление точки росы
float calculateDewPoint(float temp, float humid) {
  float a = 17.27;
  float b = 237.7;
  float alpha = ((a * temp) / (b + temp)) + log(humid / 100.0);
  return (b * alpha) / (a - alpha);
}

// Calculate the index
float calculateHeatIndex(float temp, float humid) {
  if (temp < 27) return temp;
  
  float c1 = -8.78469475556;
  float c2 = 1.61139411;
  float c3 = 2.33854883889;
  float c4 = -0.14611605;
  float c5 = -0.012308094;
  float c6 = -0.0164248277778;
  float c7 = 0.002211732;
  float c8 = 0.00072546;
  float c9 = -0.000003582;
  
  return c1 + (c2 * temp) + (c3 * humid) + (c4 * temp * humid) + 
         (c5 * temp * temp) + (c6 * humid * humid) + 
         (c7 * temp * temp * humid) + (c8 * temp * humid * humid) + 
         (c9 * temp * temp * humid * humid);
}

// Main page
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Метеостанция</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@3.9.1/dist/chart.min.js"></script>
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
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .header {
            background: white;
            border-radius: 20px;
            padding: 30px;
            margin-bottom: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            text-align: center;
        }
        
        .header h1 {
            color: #333;
            font-size: 32px;
            margin-bottom: 10px;
        }
        
        .header .subtitle {
            color: #666;
            font-size: 14px;
        }
        
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        
        .card {
            background: white;
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        
        .sensor-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            transition: transform 0.3s ease;
        }
        
        .sensor-card:hover {
            transform: translateY(-5px);
        }
        
        .humidity-card {
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
        }
        
        .sensor-label {
            font-size: 14px;
            opacity: 0.9;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }
        
        .sensor-value {
            font-size: 48px;
            font-weight: bold;
            text-align: center;
        }
        
        .sensor-unit {
            font-size: 24px;
            opacity: 0.8;
        }
        
        .minmax {
            display: flex;
            justify-content: space-around;
            margin-top: 15px;
            font-size: 12px;
            opacity: 0.9;
        }
        
        .info-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 10px;
        }
        
        .info-item {
            background: #f8f9fa;
            padding: 10px;
            border-radius: 8px;
            font-size: 13px;
        }
        
        .info-label {
            color: #666;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .info-value {
            color: #333;
            font-weight: bold;
            font-size: 16px;
            margin-top: 5px;
        }
        
        .status {
            display: inline-block;
            padding: 5px 15px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: bold;
        }
        
        .status.online {
            background: #d4edda;
            color: #155724;
        }
        
        .status.offline {
            background: #f8d7da;
            color: #721c24;
        }
        
        .chart-card {
            grid-column: 1 / -1;
        }
        
        canvas {
            max-height: 300px;
        }
        
        .buttons {
            display: flex;
            gap: 10px;
            margin-top: 15px;
            flex-wrap: wrap;
        }
        
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            cursor: pointer;
            transition: all 0.3s;
            font-weight: bold;
        }
        
        .btn-primary {
            background: #667eea;
            color: white;
        }
        
        .btn-primary:hover {
            background: #5568d3;
            transform: translateY(-2px);
        }
        
        .btn-danger {
            background: #dc3545;
            color: white;
        }
        
        .btn-danger:hover {
            background: #c82333;
            transform: translateY(-2px);
        }
        
        .btn-success {
            background: #28a745;
            color: white;
        }
        
        .btn-success:hover {
            background: #218838;
            transform: translateY(-2px);
        }
        
        .wifi-signal {
            display: inline-block;
            vertical-align: middle;
        }
        
        .update-time {
            text-align: center;
            color: #999;
            font-size: 12px;
            margin-top: 15px;
        }
        
        .temp-unit-toggle {
            margin-top: 15px;
            text-align: center;
        }
        
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 30px;
        }
        
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 30px;
        }
        
        .slider:before {
            position: absolute;
            content: "";
            height: 22px;
            width: 22px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        
        input:checked + .slider {
            background-color: #667eea;
        }
        
        input:checked + .slider:before {
            transform: translateX(30px);
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌡️ Метеостанция ESP32</h1>
            <div class="subtitle">Метеостанция ESP32</div>
            <div style="margin-top: 15px;">
                <span id="statusBadge" class="status online">🟢 Подключено</span>
            </div>
        </div>
        
        <div class="grid">
            <div class="card sensor-card">
                <div class="sensor-label">🌡️ Температура</div>
                <div class="sensor-value">
                    <span id="temperature">--</span>
                    <span class="sensor-unit" id="tempUnit">°C</span>
                </div>
                <div class="minmax">
                    <div>⬇️ Min: <span id="minTemp">--</span><span id="minTempUnit">°C</span></div>
                    <div>⬆️ Max: <span id="maxTemp">--</span><span id="maxTempUnit">°C</span></div>
                </div>
                <div class="temp-unit-toggle">
                    <label style="color: white; margin-right: 10px;">°C</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="unitToggle" onchange="toggleUnit()">
                        <span class="slider"></span>
                    </label>
                    <label style="color: white; margin-left: 10px;">°F</label>
                </div>
            </div>
            
            <div class="card sensor-card humidity-card">
                <div class="sensor-label">💧 Влажность</div>
                <div class="sensor-value">
                    <span id="humidity">--</span>
                    <span class="sensor-unit">%</span>
                </div>
                <div class="minmax">
                    <div>⬇️ Min: <span id="minHumid">--</span>%</div>
                    <div>⬆️ Max: <span id="maxHumid">--</span>%</div>
                </div>
            </div>
            
            <div class="card">
                <h3 style="margin-bottom: 15px; color: #333;">📊 Расчётные данные</h3>
                <div class="info-grid">
                    <div class="info-item">
                        <div class="info-label">Точка росы</div>
                        <div class="info-value"><span id="dewPoint">--</span> <span id="dewPointUnit">°C</span></div>
                    </div>
                    <div class="info-item">
                        <div class="info-label">Теплоощущение</div>
                        <div class="info-value"><span id="heatIndex">--</span> <span id="heatIndexUnit">°C</span></div>
                    </div>
                </div>
            </div>
            
            <div class="card">
    <h3 style="margin-bottom: 15px; color: #333;">⚙️ Система</h3>
    <div class="info-grid">
        <div class="info-item">
            <div class="info-label">Время работы</div>
            <div class="info-value" id="uptime">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">Свободно RAM</div>
            <div class="info-value" id="freeHeap">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">Использование CPU</div>
            <div class="info-value" id="cpuUsage">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">WiFi канал</div>
            <div class="info-value" id="wifiChannel">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">WiFi SSID</div>
            <div class="info-value" id="ssid">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">
                <span class="wifi-signal" id="wifiSignal">📶</span> RSSI
            </div>
            <div class="info-value" id="rssi">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">IP адрес</div>
            <div class="info-value" id="ipAddr" style="font-size: 12px;">--</div>
        </div>
        <div class="info-item">
            <div class="info-label">Переподключений</div>
            <div class="info-value" id="reconnects">--</div>
        </div>
    </div>
    
</div>
                
                <div class="buttons">
                    <button class="btn btn-primary" onclick="exportCSV()">📥 Экспорт CSV</button>
                    <button class="btn btn-success" onclick="resetMinMax()">🔄 Сброс Min/Max</button>
                    <button class="btn btn-danger" onclick="rebootDevice()">⚡ Перезагрузка</button>
                </div>
            </div>
            
            <div class="card chart-card">
                <h3 style="margin-bottom: 15px; color: #333;">📈 История (последние 3 минуты)</h3>
                <canvas id="historyChart"></canvas>
                <div class="update-time">
                    Обновлено: <span id="updateTime">--</span>
                </div>
            </div>
        </div>
    </div>

    <script>
        let isFahrenheit = false;
        let chartData = {
            labels: [],
            temp: [],
            humid: []
        };
        let chart;
        
        const ctx = document.getElementById('historyChart').getContext('2d');
        chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: chartData.labels,
                datasets: [{
                    label: 'Температура (°C)',
                    data: chartData.temp,
                    borderColor: '#667eea',
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    tension: 0.4
                }, {
                    label: 'Влажность (%)',
                    data: chartData.humid,
                    borderColor: '#4facfe',
                    backgroundColor: 'rgba(79, 172, 254, 0.1)',
                    tension: 0.4,
                    yAxisID: 'y1'
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                interaction: {
                    mode: 'index',
                    intersect: false,
                },
                scales: {
                    y: {
                        type: 'linear',
                        display: true,
                        position: 'left',
                        title: {
                            display: true,
                            text: 'Температура (°C)'
                        }
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        title: {
                            display: true,
                            text: 'Влажность (%)'
                        },
                        grid: {
                            drawOnChartArea: false,
                        },
                    },
                }
            }
        });
        
        function celsiusToFahrenheit(c) {
            return (c * 9/5) + 32;
        }
        
        function toggleUnit() {
            isFahrenheit = !isFahrenheit;
            updateDisplay();
        }
        
        function updateDisplay() {
            const units = document.querySelectorAll('#tempUnit, #minTempUnit, #maxTempUnit, #dewPointUnit, #heatIndexUnit');
            units.forEach(el => el.textContent = isFahrenheit ? '°F' : '°C');
            updateData();
        }
        
        let errorCount = 0;
        
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    if (data.error) throw new Error(data.error);
                    
                    const temp = isFahrenheit ? celsiusToFahrenheit(data.temperature) : data.temperature;
                    const minT = isFahrenheit ? celsiusToFahrenheit(data.minTemp) : data.minTemp;
                    const maxT = isFahrenheit ? celsiusToFahrenheit(data.maxTemp) : data.maxTemp;
                    const dewP = isFahrenheit ? celsiusToFahrenheit(data.dewPoint) : data.dewPoint;
                    const heatI = isFahrenheit ? celsiusToFahrenheit(data.heatIndex) : data.heatIndex;
                    
                    document.getElementById('temperature').textContent = temp.toFixed(1);
                    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
                    document.getElementById('minTemp').textContent = minT.toFixed(1);
                    document.getElementById('maxTemp').textContent = maxT.toFixed(1);
                    document.getElementById('minHumid').textContent = data.minHumid.toFixed(1);
                    document.getElementById('maxHumid').textContent = data.maxHumid.toFixed(1);
                    document.getElementById('dewPoint').textContent = dewP.toFixed(1);
                    document.getElementById('heatIndex').textContent = heatI.toFixed(1);
                    
                    const now = new Date();
                    document.getElementById('updateTime').textContent = now.toLocaleTimeString('ru-RU');
                    
                    errorCount = 0;
                    document.getElementById('statusBadge').className = 'status online';
                    document.getElementById('statusBadge').innerHTML = '🟢 Подключено';
                })
                .catch(error => {
                    console.error('Ошибка:', error);
                    errorCount++;
                    if (errorCount > 2) {
                        document.getElementById('statusBadge').className = 'status offline';
                        document.getElementById('statusBadge').innerHTML = '🔴 Ошибка';
                    }
                });
        }
        
        function updateStats() {
    fetch('/stats')
        .then(response => response.json())
        .then(data => {
            document.getElementById('uptime').textContent = data.uptime;
            document.getElementById('freeHeap').textContent = data.freeHeap;
            document.getElementById('cpuUsage').textContent = data.cpuUsage + '%';
            document.getElementById('wifiChannel').textContent = data.wifiChannel;
            document.getElementById('ssid').textContent = data.ssid;
            document.getElementById('rssi').textContent = data.rssi + ' dBm';
            document.getElementById('ipAddr').textContent = data.ip;
            document.getElementById('reconnects').textContent = data.reconnects;
            
            const rssi = parseInt(data.rssi);
            let signal = '📶';
            if (rssi > -50) signal = '📶';
            else if (rssi > -60) signal = '📶';
            else if (rssi > -70) signal = '📡';
            else signal = '📉';
            document.getElementById('wifiSignal').textContent = signal;
        });
}
        
        function updateHistory() {
            fetch('/history')
                .then(response => response.json())
                .then(data => {
                    chartData.labels = data.labels;
                    chartData.temp = data.temp;
                    chartData.humid = data.humid;
                    
                    chart.data.labels = chartData.labels;
                    chart.data.datasets[0].data = chartData.temp;
                    chart.data.datasets[1].data = chartData.humid;
                    chart.update('none');
                });
        }
        
        function resetMinMax() {
            if (confirm('Сбросить минимальные и максимальные значения?')) {
                fetch('/reset')
                    .then(response => response.json())
                    .then(data => {
                        alert(data.message || 'Min/Max сброшены');
                        updateData();
                    });
            }
        }
        
        function rebootDevice() {
            if (confirm('⚠️ ВНИМАНИЕ! Устройство будет перезагружено. Продолжить?')) {
                fetch('/reboot')
                    .then(() => {
                        alert('Устройство перезагружается... Подождите 10 секунд.');
                        setTimeout(() => location.reload(), 10000);
                    });
            }
        }
        
        function exportCSV() {
            let csv = 'Время,Температура (°C),Влажность (%)\n';
            for (let i = 0; i < chartData.labels.length; i++) {
                csv += `${chartData.labels[i]},${chartData.temp[i]},${chartData.humid[i]}\n`;
            }
            
            const blob = new Blob([csv], { type: 'text/csv' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `esp32_data_${new Date().toISOString().slice(0,10)}.csv`;
            a.click();
        }
        
        updateData();
        updateStats();
        updateHistory();
        
        setInterval(updateData, 3000);
        setInterval(updateStats, 5000);
        setInterval(updateHistory, 3000);
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

// API: Sensor's data
void handleData() {
  if (isnan(temperature) || isnan(humidity)) {
    server.send(500, "application/json", "{\"error\":\"Данные не готовы\"}");
    return;
  }
  
  float dewPoint = calculateDewPoint(temperature, humidity);
  float heatIndex = calculateHeatIndex(temperature, humidity);
  
  String json = "{";
  json += "\"temperature\":" + String(temperature, 2) + ",";
  json += "\"humidity\":" + String(humidity, 2) + ",";
  json += "\"minTemp\":" + String(minTemp, 2) + ",";
  json += "\"maxTemp\":" + String(maxTemp, 2) + ",";
  json += "\"minHumid\":" + String(minHumid, 2) + ",";
  json += "\"maxHumid\":" + String(maxHumid, 2) + ",";
  json += "\"dewPoint\":" + String(dewPoint, 2) + ",";
  json += "\"heatIndex\":" + String(heatIndex, 2);
  json += "}";
  
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// API: Stats of the system
void handleStats() {
  unsigned long uptime = (millis() - bootTime) / 1000;
  int hours = uptime / 3600;
  int minutes = (uptime % 3600) / 60;
  int seconds = uptime % 60;
  
  char uptimeStr[20];
  sprintf(uptimeStr, "%02d:%02d:%02d", hours, minutes, seconds);
  
  // Using RAM
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t usedHeap = totalHeap - freeHeap;
  float heapUsagePercent = (float)usedHeap / totalHeap * 100.0;
  
  String json = "{";
  json += "\"uptime\":\"" + String(uptimeStr) + "\",";
  json += "\"freeHeap\":\"" + String(freeHeap / 1024) + " KB (" + String(heapUsagePercent, 1) + "%)\",";
  json += "\"cpuUsage\":\"" + String(cpuUsage, 1) + "\",";
  json += "\"wifiChannel\":" + String(WiFi.channel()) + ",";
  json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
  json += "\"rssi\":\"" + String(WiFi.RSSI()) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"reconnects\":" + String(wifiReconnects);
  json += "}";
  
  server.send(200, "application/json", json);
}

// API: Hystorical data
void handleHistory() {
  String json = "{";
  json += "\"labels\":[";
  for (int i = 0; i < historySize; i++) {
    if (i > 0) json += ",";
    json += "\"" + String(i * 3) + "s\"";
  }
  json += "],\"temp\":[";
  for (int i = 0; i < historySize; i++) {
    if (i > 0) json += ",";s
    json += String(tempHistory[i], 1);
  }
  json += "],\"humid\":[";
  for (int i = 0; i < historySize; i++) {
    if (i > 0) json += ",";
    json += String(humidHistory[i], 1);
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

// API: Reset min/max
void handleReset() {
  minTemp = temperature;
  maxTemp = temperature;
  minHumid = humidity;
  maxHumid = humidity;
  
  server.send(200, "application/json", "{\"message\":\"Min/Max значения сброшены\"}");
  Serial.println("Min/Max сброшены");
}

// API: Reboot
void handleReboot() {
  server.send(200, "application/json", "{\"message\":\"Перезагрузка...\"}");
  Serial.println("Перезагрузка по запросу пользователя");
  delay(1000);
  ESP.restart();
}