# Arquitectura del proyecto Baymax

## Estructura del repositorio

- `index.html`  
  Panel web principal para GitHub Pages y modo demostración.

- `esp32/Baymax_Expo.ino`  
  Firmware base del ESP32 para la exposición.

- `docs/ARQUITECTURA.md`  
  Este documento.

## Flujo general

1. **MLX90614** mide temperatura.
2. **HX711 + 4 celdas de carga** obtiene el peso.
3. **VL53L1X** mide la distancia desde el punto fijo superior hasta la cabeza y permite calcular altura.
4. El **ESP32** procesa las lecturas.
5. La web calcula y muestra el **IMC**.
6. **HC-SR04** detecta obstáculos para movilidad.
7. **TB6612FNG** controla los dos motores TT.
8. **DFPlayer Mini + bocina** reproduce frases.
9. **Buzzer** emite avisos cortos.
10. **OLED** muestra información directamente en Baymax.
11. Presión arterial y glucosa se registran como valores externos.

## Uso durante la exposición

El ESP32 crea una red propia:

- SSID: `Baymax-Expo`
- Contraseña: `BaymaxExpo2026`
- Dirección local: `http://192.168.4.1`

Esto permite manejar Baymax sin depender del Wi-Fi escolar.

## Orden recomendado de integración

1. ESP32
2. OLED
3. Botonera
4. HC-SR04
5. TB6612FNG + motores
6. MLX90614
7. VL53L1X
8. HX711 + celdas
9. Buzzer
10. DFPlayer Mini
11. Integración completa con web

## Nota

Los pines incluidos en el firmware son una **propuesta inicial**. Antes del cableado final deben verificarse contra la placa ESP32 exacta y contra los módulos físicos adquiridos.
