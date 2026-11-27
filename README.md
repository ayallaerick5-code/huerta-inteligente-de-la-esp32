# Huerta Inteligente (ESP32)

Sistema de monitoreo ambiental y control automatizado de riego e iluminación para huerta usando un **ESP32**, pantalla **OLED SSD1306 (I2C)**, sensor **DHT11**, sensor de **humedad de suelo**, sensor de **luz** y módulo de **2 relés**.

## Componentes utilizados
- Microcontrolador **ESP32**
- Pantalla **OLED SSD1306 128x64 (I2C)**
- Sensor de temperatura y humedad ambiental **DHT11**
- Sensor de **humedad del suelo**
- Sensor de **luz**
- Módulo de **2 relés** (disparo en bajo: `LOW` encendido / `HIGH` apagado)
- Bomba de agua (controlada por Relé 1) e iluminación/actuador (Relé 2)

## Conexiones (Pines GPIO)
| Componente | Pin / Conexión ESP32 |
| :--- | :--- |
| **DHT11** | GPIO 4 |
| **Humedad del suelo (ADC)** | GPIO 34 |
| **Sensor de luz (ADC)** | GPIO 35 |
| **Relé 1 (Bomba de agua)** | GPIO 26 |
| **Relé 2** | GPIO 27 |
| **OLED SSD1306 (I2C)** | SDA = GPIO 21, SCL = GPIO 22 |

## Librerías necesarias
- `Wire.h`
- `U8g2lib.h` (U8g2)
- `DHT.h` (DHT sensor library)

## Funcionamiento principal
- Lee sensores cada **2 segundos** (y promedia 10 lecturas para la humedad del suelo).
- La **bomba de agua (Relé 1)** se activa cada **50 segundos** durante **5 segundos** (con `millis()`) y también cuenta con respaldo si la humedad del suelo baja del **40 %**.
- El **Relé 2** se activa si el nivel de luz baja del **30 %**.
- Muestra en la pantalla OLED las variables ambientales, el estado de los relés y pantallas informativas (incluyendo **HUERTA INTELIGENTE :)**).

## Autores
- Erik Quiroga
- David Quiroga
