# Arquitectura final · Baymax

## 1. Unidad principal

El ESP32 controla la interfaz, sensores, audio y movimiento.

La alimentación se divide en dos:
- **Power bank ADATA → USB-C del ESP32** para lógica, sensores y periféricos.
- **4 AA → interruptor → VM del TB6612** para motores.

Todas las tierras eléctricas necesarias comparten GND.

## 2. Interfaz física

OLED SSD1309 128x64 en el pecho.

Tres pulsadores:
- ▲ GPIO4
- OK GPIO5
- ▼ GPIO13

El menú físico ofrece temperatura, altura, IMC, distancia y estado.

## 3. Sensores

### MLX90614
Comparte I2C:
- SDA GPIO21
- SCL GPIO22
- dirección 0x5A

### VL53L1X
Sensor desmontable ubicado arriba del poste de altura:
- SDA GPIO21
- SCL GPIO22
- dirección 0x29

Cálculo:

`altura de persona = altura fija del sensor - distancia ToF a la cabeza`

### HC-SR04
- TRIG GPIO18
- ECHO GPIO19 mediante divisor 1 kΩ / 2 kΩ
- detención automática a 20 cm o menos

## 4. Peso e IMC

Para la expo se descartó la construcción de la báscula con celdas de carga.

Se usa una **báscula comercial externa**. El peso se captura:
- desde el menú OLED con ▲/▼ y OK, o
- desde el panel web del ESP32.

El ESP32 combina el peso introducido con la última altura medida y calcula el IMC.

## 5. Movimiento

TB6612FNG:
- PWMA 25
- AIN1 26
- AIN2 27
- PWMB 33
- BIN1 32
- BIN2 14
- STBY fijo a 3.3V

El interruptor físico alimenta o corta VM. El movimiento se ordena desde el panel web.

## 6. Audio

### Buzzer pasivo
GPIO17, controlado mediante LEDC.

### DFPlayer Mini
UART2:
- ESP32 RX GPIO16 ← TX DFPlayer
- ESP32 TX GPIO23 → resistencia 1 kΩ → RX DFPlayer

## 7. Panel web

El ESP32 crea:
- SSID: `Baymax-Expo`
- contraseña: `BaymaxExpo2026`
- página local: `http://192.168.4.1`

Desde ahí se puede:
- mover y detener el robot;
- consultar lecturas;
- medir temperatura;
- medir altura;
- introducir el peso de la báscula externa;
- obtener el IMC.

## 8. Seguridad funcional

- El HC-SR04 frena el robot ante un obstáculo.
- El ToF es desmontable: el poste no debe permanecer conectado mientras Baymax se desplaza.
- Las mediciones son demostrativas y no diagnósticas.
