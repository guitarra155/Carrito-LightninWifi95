/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/

#include <WiFi.h>
#include <PubSubClient.h>
#define BUILTIN_LED 2

// Update these with values suitable for your network.

const char *ssid = "LightninWifi95";
const char *password = "LightninWifi95";
const char *mqtt_server = "192.168.0.100";

// --- Definiciones L298N ---
// Motor A (Traccion - Trasero)
const int ENA = 32; // Habilitar A (GPIO 14)
const int IN1 = 33;
const int IN2 = 25;

// Motor B (Direccion - Delantero)
const int ENB = 14; // Habilitar B (GPIO 32)
const int IN3 = 26;
const int IN4 = 27;

// --- Sensor Infrarrojo (TCRT5000) - Meta/Vueltas ---
const int SENSOR_PIN = 13;
bool detectingLine = false;
unsigned long sensorStartTime = 0;

// Variables de Carrera
int lapCount = 0;                // Contador de vueltas (0 = No iniciado)
unsigned long raceStartTime = 0; // Tiempo en que inicio la carrera (Lap 1)
unsigned long lastLapTime = 0;   // Tiempo de la ultima vuelta
const int MIN_LAP_TIME = 2000;   // Tiempo minimo entre vueltas (para evitar rebotes)

// Configuración PWM
const int pwmFreq = 30000;
const int pwmResolution = 8;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// --- Turbo Mode Variables ---
bool turboMode = false;
int lastSpeedA = 0;
bool lastReverseA = false;
unsigned long connectionStart = 0;

void setupMotors()
{
  // Configurar pines de direccion como salida
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Configurar pin del sensor
  pinMode(SENSOR_PIN, INPUT);

  // Inicializar en apagado
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  // Configurar PWM para velocidad (ESP32 Core 3.0+)
  ledcAttach(ENA, pwmFreq, pwmResolution);
  ledcAttach(ENB, pwmFreq, pwmResolution);

  // Escribir 0 inicialmente
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
}

// Control Motor A: Aceleracion y Retroceso
// speed: 0-1000 map to 100-255 (para evitar zona muerta)
void controlMotorA(int speed, bool reverse)
{
  int pwmValue = 0;

  // Guardar estado para actualizaciones de Turbo
  lastSpeedA = speed;
  lastReverseA = reverse;

  int maxPWM = turboMode ? 255 : 200;

  // map(value, fromLow, fromHigh, toLow, toHigh)
  pwmValue = map(speed, 0, 1000, 135, maxPWM);

  // Asegurar limites
  if (pwmValue < 0)
    pwmValue = 0;
  if (pwmValue > maxPWM)
    pwmValue = maxPWM;

  ledcWrite(ENA, pwmValue);

  if (reverse)
  {
    // Retroceso
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }
  else
  {
    // Adelante
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  }
}

// Control Motor B: Direccion
// axisValue: -1000 (Izquierda) a +1000 (Derecha)
void controlMotorB(int axisValue)
{
  // Definir zona muerta
  int deadZone = 10;

  int absValue = abs(axisValue);
  int pwmValue = 0;

  // Direccion suele necesitar mas fuerza inicial (torque) para vencer el resorte
  if (absValue > deadZone)
  {
    // Mapear de 200 a 255 (Fuerza considerable)
    pwmValue = map(absValue, deadZone, 1000, 100, 255);
  }

  if (pwmValue > 255)
    pwmValue = 255;

  // Debug para ver si entra a la logica
  // Serial.print("Steering: "); Serial.print(axisValue); Serial.print(" PWM: "); Serial.println(pwmValue);

  ledcWrite(ENB, pwmValue);

  if (axisValue > deadZone)
  {
    // Derecha (+1000)
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }
  else if (axisValue < -deadZone)
  {
    // Izquierda (-1000)
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }
  else
  {
    // Centro / Parar
    ledcWrite(ENB, 0);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }
}

void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Parse payload as string
  String message = "";
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  String topicStr = String(topic);
  int msgVal = message.toInt(); // Convertir payload a entero para uso en motores

  if (topicStr.indexOf("hat") != -1)
  {
    // Format: "x,y"
    int commaIndex = message.indexOf(',');
    if (commaIndex != -1)
    {
      String xStr = message.substring(0, commaIndex);
      String yStr = message.substring(commaIndex + 1);
      int x = xStr.toInt();
      int y = yStr.toInt();

      Serial.print("[CRUCETA] X: ");
      Serial.print(x);
      Serial.print(" Y: ");
      Serial.print(y);

      if (y == 1)
        Serial.print(" (ARRIBA)");
      if (y == -1)
        Serial.print(" (ABAJO)");
      if (x == -1)
        Serial.print(" (IZQUIERDA)");
      if (x == 1)
        Serial.print(" (DERECHA)");
      if (x == 0 && y == 0)
        Serial.print(" (CENTRO)");
      Serial.println();
    }
  }
  else if (topicStr.indexOf("boton") != -1)
  {
    Serial.print("[BOTON] ID: ");
    Serial.print(topicStr.substring(topicStr.lastIndexOf('/') + 1));
    Serial.print(" Valor: ");
    Serial.println(message);

    // Button 0: Turbo Mode
    if (topicStr.endsWith("/boton/0"))
    {
      int btnVal = message.toInt();
      if (btnVal == 1)
      {
        turboMode = true;
        Serial.println("TURBO ON");
      }
      else
      {
        turboMode = false;
        Serial.println("TURBO OFF");
      }
      // Update speed immediately if moving
      if (lastSpeedA > 0)
      {
        controlMotorA(lastSpeedA, lastReverseA);
      }
    }
  }
  else if (topicStr.indexOf("eje") != -1)
  {
    String axisIdStr = topicStr.substring(topicStr.lastIndexOf('/') + 1);
    int axisId = axisIdStr.toInt();

    // Imprimir info Eje
    if (axisId == 4 || axisId == 5)
    {
      Serial.print("[GATILLO] ID: ");
    }
    else
    {
      Serial.print("[JOYSTICK] ID: ");
    }
    Serial.print(axisId);
    Serial.print(" Valor: ");
    Serial.println(message);

    // Enviar a Node-RED para monitoreo
    String monitorTopic = "carrito/monitor/eje/" + String(axisId);
    client.publish(monitorTopic.c_str(), message.c_str());

    // ID 5: Gatillo Derecho - Acelerador (0 a 1000 segun prompt)
    if (axisId == 5)
    {
      if (msgVal > 0)
      {
        controlMotorA(msgVal, false); // Adelante
      }
      else
      {
        controlMotorA(0, false);
      }
    }
    // ID 4: Gatillo Izquierdo - Retroceso (0 a 1000)
    else if (axisId == 4)
    {
      if (msgVal > 0)
      {
        controlMotorA(msgVal, true); // Atras
      }
      else
      {
        controlMotorA(0, true);
      }
    }
    // ID 0: Joystick Izquierdo X - Direccion
    else if (axisId == 0)
    {
      controlMotorB(msgVal);
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");

      connectionStart = millis();                         // Reset timer
      client.publish("carrito/estado", "Conectado (0s)"); // Enviar estado inicial

      // ... and resubscribe
      client.subscribe("mando/#");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// --- Funcion Lógica del Sensor ---
void checkSensor()
{
  int currentSensorState = digitalRead(SENSOR_PIN);
  unsigned long now = millis();

  // Detectar Flanco de Subida (Entrada)
  if (currentSensorState == HIGH && !detectingLine)
  {
    detectingLine = true;
    sensorStartTime = now;
  }
  // Detectar Flanco de Bajada (Salida)
  else if (currentSensorState == LOW && detectingLine)
  {
    detectingLine = false;
    unsigned long crossingDuration = now - sensorStartTime;

    if (crossingDuration > 50)
    {
      if (lapCount == 0 || (now - lastLapTime > MIN_LAP_TIME))
      {
        lapCount++;
        String eventType = "lap";
        unsigned long currentLapTime = 0;
        unsigned long totalTime = 0;

        if (lapCount == 1)
        {
          eventType = "start";
          raceStartTime = now;
          lastLapTime = now;
          Serial.println("!!! INICIO DE CARRERA !!!");
        }
        else
        {
          currentLapTime = now - lastLapTime;
          totalTime = now - raceStartTime;
          lastLapTime = now;
          Serial.print("VUELTA ");
          Serial.print(lapCount);
          Serial.print(" - Tiempo: ");
          Serial.print(currentLapTime / 1000.0);
          Serial.println("s");
        }

        String jsonPayload = "{";
        jsonPayload += "\"lap\":" + String(lapCount) + ",";
        jsonPayload += "\"event\":\"" + eventType + "\",";
        jsonPayload += "\"lap_time_ms\":" + String(currentLapTime) + ",";
        jsonPayload += "\"total_time_ms\":" + String(totalTime) + ",";
        jsonPayload += "\"crossing_duration_ms\":" + String(crossingDuration);
        jsonPayload += "}";

        client.publish("carrito/sensor", jsonPayload.c_str());
      }
    }
  }
}

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);
  Serial.begin(115200);
  setupMotors(); // Inicializar motores
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  checkSensor();

  // Enviar estado periodicamente (Heartbeat)
  unsigned long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;

    unsigned long uptimeSeconds = (now - connectionStart) / 1000;
    String statusMsg = "Conectado (" + String(uptimeSeconds) + "s)";

    // Serial.println("Enviando Heartbeat...");
    client.publish("carrito/estado", statusMsg.c_str());
  }
}
