🏎️ Carrito-LightninWifi95 🚀
Este proyecto consiste en un vehículo robótico controlado por WiFi mediante el protocolo MQTT, equipado con una cámara en tiempo real y un sistema de telemetría avanzado. El sistema combina hardware basado en ESP32, control por mando (joystick) y una interfaz de monitoreo.

🌟 Características Principales
🎮 Control por Mando: Script en Python que captura la entrada de un mando (Xbox/PlayStation/Genérico) y traduce los ejes y botones a comandos MQTT en tiempo real.
📡 Comunicación Robusta: Utiliza un broker Mosquitto para una comunicación de baja latencia entre el mando y el vehículo.
🚀 Modo Turbo: Activación dinámica de potencia extra mediante botones del mando.
⏱️ Sistema de Vueltas (Laps): Sensor infrarrojo TCRT5000 integrado para detectar cruces por meta, calculando tiempos de vuelta y estadísticas de carrera enviadas automáticamente por JSON.
📸 Streaming de Video: Servidor web dedicado en una ESP32-CAM para visualización en vivo del trayecto.
📊 Dashboard de Telemetría: Archivos de configuración para Node-RED, permitiendo visualizar en un panel web la velocidad, el estado de los ejes y los tiempos de carrera.
🛠️ Estructura del Proyecto
1. 🏎️ Carrito/ (Cerebro del Vehículo)
Carrito.ino
: Firmware para ESP32 que gestiona los motores (L298N), el sensor de línea y la suscripción a los comandos MQTT.
MandoMQTT.py
: Cliente Python que utiliza pygame y paho-mqtt para convertir tu control físico en un mando remoto WiFi.
Dashboard/: Archivos 
.json
 listos para importar en Node-RED y tener un monitor profesional del estado del carro.
2. 📷 CameraWebServer/ (Ojo del Piloto)
Firmware optimizado para ESP32-CAM (AI-Thinker) que levanta un servidor de video en streaming estable para pilotar en primera persona.
🚀 Requisitos Rápidos
Hardware: ESP32, ESP32-CAM, Driver L298N, 2 Motores DC, Sensor Infrarrojo TCRT5000.
Software:
Arduino IDE (con soporte para ESP32).
Python 3.12+ (para el script del mando).
Broker MQTT (como Mosquitto).
Node-RED (opcional, para el dashboard).
🔧 Configuración Rápida
Sube los archivos 
.ino
 a sus respectivas placas ESP32.
Asegúrate de que todos los dispositivos estén en la red WiFi LightninWifi95.
Ejecuta el script de control:
bash
python MandoMQTT.py
¡A correr! 🏁
Nota: Este proyecto fue diseñado para ofrecer una experiencia de conducción fluida y un sistema de competición integrado mediante el análisis de datos en tiempo real.

Hecho con ❤️ por [Tu Nombre/Usuario de GitHub]
