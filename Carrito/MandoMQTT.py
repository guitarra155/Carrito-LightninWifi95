import pygame
import paho.mqtt.client as mqtt
import time
import sys


# para ejecutar con python 3.12 
# py -3.12 MandoMQTT.py
# py -3.12 -m pip install paho-mqtt


# --- CONFIGURACIÓN ---
BROKER = "192.168.0.101"
PORT = 1883
TOPIC_PREFIX = "mando"

# Configuración de Ejes
# Triggers: mapeo de -1..1 a 0..1000
GATILLOS_INDICES = [4, 5] 

# Umbral de zona muerta y cambio mínimo para enviar MQTT
DEADZONE = 0.08
MIN_CHANGE = 2  # Solo enviar si el valor cambia en al menos 2 unidades (para evitar spam)

# --- MQTT SETUP ---
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print(f"Conectado al broker MQTT con código: {rc}")

client.on_connect = on_connect

try:
    client.connect(BROKER, PORT, 60)
except Exception as e:
    print(f"Error conectando al broker: {e}")
    sys.exit(1)

# Usamos loop_start para manejar la red en un hilo separado (no bloqueante)
client.loop_start()

# --- PYGAME SETUP ---
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("No se detectó ningún mando. Conecta uno y reinicia.")
    sys.exit()

joystick = pygame.joystick.Joystick(0)
joystick.init()

print(f"Mando detectado: {joystick.get_name()}")
print(f"Ejes detectados: {joystick.get_numaxes()}")
print(f"Botones detectados: {joystick.get_numbuttons()}")
print("--- Iniciando captura en tiempo real ---")

# Diccionario para guardar el último valor enviado y no saturar la red
last_values = {}

try:
    running = True
    clock = pygame.time.Clock()
    
    while running:
        # Limitar el bucle a 120 FPS para no consumir 100% CPU
        # Esto reduce el lag general del sistema
        dt = clock.tick(60) 

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            
            # --- BOTONES (Prioridad alta, se envían siempre) ---
            elif event.type == pygame.JOYBUTTONDOWN:
                client.publish(f"{TOPIC_PREFIX}/boton/{event.button}", "1")
                print(f"Boton {event.button} PRESIONADO (Enviado)")
                
            elif event.type == pygame.JOYBUTTONUP:
                client.publish(f"{TOPIC_PREFIX}/boton/{event.button}", "0")
                print(f"Boton {event.button} LIBERADO (Enviado)")

            # --- EJES Y GATILLOS ---
            elif event.type == pygame.JOYAXISMOTION:
                axis_id = event.axis
                raw_val = joystick.get_axis(axis_id)
                
                # 1. Aplicar Deadzone
                if abs(raw_val) < DEADZONE:
                    raw_val = 0.0
                
                # 2. Convertir valor
                val_final = 0
                if axis_id in GATILLOS_INDICES:
                    # Trigger: -1..1 -> 0..1000
                    # Asegurar que esté entre -1 y 1 antes de operar
                    raw_val = max(-1.0, min(1.0, raw_val))
                    val_final = int(((raw_val + 1) / 2) * 1000)
                else:
                    # Joystick: -1..1 -> -1000..1000
                    val_final = int(raw_val * 1000)

                # 3. Filtrado de ruido (Solo enviar si cambió lo suficiente)
                last_val = last_values.get(f"axis_{axis_id}", -999)
                
                if abs(val_final - last_val) >= MIN_CHANGE:
                    topic = f"{TOPIC_PREFIX}/eje/{axis_id}"
                    client.publish(topic, str(val_final))
                    last_values[f"axis_{axis_id}"] = val_final
                    
                    tipo = "Gatillo" if axis_id in GATILLOS_INDICES else "Joystick"
                    print(f"{tipo} {axis_id}: {val_final} (Enviado)") 

            # --- HATS ---
            elif event.type == pygame.JOYHATMOTION:
                hat_x, hat_y = joystick.get_hat(event.hat)
                topic = f"{TOPIC_PREFIX}/hat/{event.hat}"
                msg = f"{hat_x},{hat_y}"
                
                if last_values.get(f"hat_{event.hat}") != msg:
                    client.publish(topic, msg)
                    last_values[f"hat_{event.hat}"] = msg
                    print(f"Cruceta {event.hat}: {msg} (Enviado)")

except KeyboardInterrupt:
    print("\nSaliendo...")

finally:
    client.loop_stop()
    client.disconnect()
    pygame.quit()
