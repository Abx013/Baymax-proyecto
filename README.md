# Baymax · Proyecto semestral de electrónica

Firmware y panel de exposición para el prototipo Baymax con ESP32.

## Estado final de la expo

### Funciones integradas
- OLED SSD1309 128x64 por I2C.
- MLX90614 para temperatura superficial.
- VL53L1X desmontable para medir altura desde un poste.
- HC-SR04 para detectar obstáculos.
- TB6612FNG + dos motores 2WD.
- DFPlayer Mini + bocina.
- Buzzer pasivo.
- 3 pulsadores físicos para navegar el menú: **ARRIBA / OK / ABAJO**.
- Báscula comercial externa: el peso se captura manualmente para calcular el IMC.
- WiFi propio del ESP32 para controlar movimiento desde celular o laptop.

> El interruptor físico de Baymax no va a un GPIO. Está en serie entre el positivo de las 4 AA y `VM` del TB6612; sirve para dar o cortar energía a los motores.

## Firmware

Archivo principal:

`esp32/Baymax_Expo.ino`

### Placa
Seleccionar en Arduino IDE:

`ESP32 Dev Module`

### Librerías necesarias
Instalar desde Library Manager:
- **U8g2**
- **Adafruit MLX90614 Library**
- **Adafruit VL53L1X**
- **DFRobotDFPlayerMini**

`WiFi`, `WebServer` y `Wire` vienen con el paquete ESP32.

## Pines finales

| Función | GPIO |
|---|---:|
| TB6612 PWMA | 25 |
| TB6612 AIN1 | 26 |
| TB6612 AIN2 | 27 |
| TB6612 PWMB | 33 |
| TB6612 BIN1 | 32 |
| TB6612 BIN2 | 14 |
| Buzzer | 17 |
| DFPlayer RX ESP32 | 16 |
| DFPlayer TX ESP32 | 23 |
| I2C SDA | 21 |
| I2C SCL | 22 |
| HC-SR04 TRIG | 18 |
| HC-SR04 ECHO | 19 |
| Botón ARRIBA | 4 |
| Botón OK | 5 |
| Botón ABAJO | 13 |

### HC-SR04
- VCC → 5V.
- GND → GND común.
- TRIG → GPIO18.
- ECHO → 1 kΩ → GPIO19.
- Desde GPIO19 → 2 kΩ → GND.

### Bus I2C compartido
OLED, MLX90614 y VL53L1X comparten:
- SDA → GPIO21
- SCL → GPIO22

El firmware fuerza el bus a 100 kHz.

## Menú físico OLED

Los tres botones funcionan como:

- GPIO4 → ▲ ARRIBA
- GPIO5 → OK
- GPIO13 → ▼ ABAJO

Opciones:
1. Temperatura
2. Altura
3. IMC
4. Distancia
5. Estado

### IMC
1. Primero mide la altura con el ToF.
2. Lee el peso en la báscula comercial externa.
3. Entra a **IMC**.
4. Usa ▲/▼ para colocar el peso.
5. Presiona OK para calcular.

El prototipo muestra únicamente el valor numérico del IMC; no realiza diagnóstico.

## Poste del VL53L1X

Antes de la expo mide la distancia real desde el piso hasta la cara del sensor y cambia esta línea:

```cpp
float ALTURA_SENSOR_CM = 200.0;
```

Ejemplo: si el sensor queda a 198.5 cm:

```cpp
float ALTURA_SENSOR_CM = 198.5;
```

## Movimiento desde celular

El ESP32 crea su propia red:

- WiFi: **Baymax-Expo**
- Contraseña: **BaymaxExpo2026**
- Dirección: **http://192.168.4.1**

1. Enciende el ESP32 con la power bank ADATA.
2. Coloca las 4 AA.
3. Pon el interruptor de motores en ON.
4. Conecta el celular a `Baymax-Expo`.
5. Abre `http://192.168.4.1`.
6. Usa los controles de movimiento.

El HC-SR04 detiene automáticamente los motores si detecta un obstáculo a 20 cm o menos.

## Audios DFPlayer

Carpeta de la microSD:

`/mp3/`

Archivos:
- `0001.mp3` saludo
- `0002.mp3` inicio de evaluación
- `0003.mp3` midiendo temperatura
- `0004.mp3` midiendo peso y altura
- `0005.mp3` calculando IMC
- `0006.mp3` evaluación terminada
- `0007.mp3` obstáculo detectado
- `0008.mp3` despedida

## Alimentación

- Power bank ADATA → USB-C del ESP32.
- 4 AA → interruptor físico → `VM` del TB6612.
- Negativo de 4 AA → GND común.
- TB6612 VCC y STBY → 3.3V.

## Nota

Prototipo escolar y educativo. Las lecturas no sustituyen dispositivos médicos certificados ni valoración profesional.
