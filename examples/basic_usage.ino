/*
 * ESP32 + AHT10 - Базовый пример использования
 * 
 * Этот пример показывает минимальный код для работы с AHT10
 * без веб-сервера - только вывод в Serial Monitor
 * 
 * Подключение:
 * AHT10 VIN -> ESP32 3.3V
 * AHT10 GND -> ESP32 GND
 * AHT10 SDA -> ESP32 GPIO 21
 * AHT10 SCL -> ESP32 GPIO 22
 */

#include <Wire.h>
#include <Adafruit_AHTX0.h>

// Создаем объект датчика
Adafruit_AHTX0 aht;

void setup() {
  // Инициализация Serial порта
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("ESP32 + AHT10 - Базовый пример");
  Serial.println("=================================");
  
  // Инициализация I2C
  Wire.begin();
  
  // Инициализация датчика AHT10
  Serial.print("Инициализация AHT10... ");
  
  if (!aht.begin()) {
    Serial.println("ОШИБКА!");
    Serial.println("Не удалось найти датчик AHT10.");
    Serial.println("Проверьте подключение:");
    Serial.println("  VIN -> 3.3V");
    Serial.println("  GND -> GND");
    Serial.println("  SDA -> GPIO 21");
    Serial.println("  SCL -> GPIO 22");
    
    // Останавливаем выполнение
    while (1) {
      delay(10);
    }
  }
  
  Serial.println("OK!");
  Serial.println("Датчик успешно инициализирован!");
  Serial.println();
  Serial.println("Начинаю считывание данных...");
  Serial.println("---------------------------------");
}

void loop() {
  // Создаем переменные для хранения данных
  sensors_event_t humidity, temperature;
  
  // Считываем данные с датчика
  aht.getEvent(&humidity, &temperature);
  
  // Выводим данные в Serial Monitor
  Serial.print("Температура: ");
  Serial.print(temperature.temperature, 1);
  Serial.print(" °C  |  ");
  
  Serial.print("Влажность: ");
  Serial.print(humidity.relative_humidity, 1);
  Serial.println(" %");
  
  // Дополнительная информация для отладки
  printComfortLevel(temperature.temperature, humidity.relative_humidity);
  
  Serial.println("---------------------------------");
  
  // Задержка перед следующим измерением (2 секунды)
  delay(2000);
}

// Функция для определения уровня комфорта
void printComfortLevel(float temp, float hum) {
  Serial.print("Уровень комфорта: ");
  
  // Оптимальная температура: 20-24°C
  // Оптимальная влажность: 40-60%
  
  if (temp >= 20 && temp <= 24 && hum >= 40 && hum <= 60) {
    Serial.println("✓ КОМФОРТНО");
  } else if (temp < 18) {
    Serial.println("❄ ХОЛОДНО");
  } else if (temp > 26) {
    Serial.println("🔥 ЖАРКО");
  } else if (hum < 30) {
    Serial.println("⚠ СУХОЙ ВОЗДУХ");
  } else if (hum > 70) {
    Serial.println("💧 ВЛАЖНЫЙ ВОЗДУХ");
  } else {
    Serial.println("~ ПРИЕМЛЕМО");
  }
}
