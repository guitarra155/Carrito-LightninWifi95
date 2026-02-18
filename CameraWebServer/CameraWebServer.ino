#include "esp_camera.h"
#include <WiFi.h>

// ==========================================
// 1. CONFIGURACIÓN DE PINES (MODELO AI THINKER)
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==========================================
// 2. CREDENCIALES WIFI
// ==========================================
const char *ssid = "LightninWifi95";
const char *password = "LightninWifi95";

// ==========================================
// 3. DECLARACIÓN DE FUNCIONES DEL SERVIDOR
// (Estas funciones están en el archivo app_httpd.cpp 
//  que viene con el ejemplo original)
// ==========================================
void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  // Pequeña pausa para estabilizar energía
  delay(2000);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // 20MHz es más estable que 24MHz
  config.pixel_format = PIXFORMAT_JPEG; 
  
  // Configuración de memoria (PSRAM)
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST; 
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  // ==========================================
  // 4. INICIALIZACIÓN DE CÁMARA
  // ==========================================
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    Serial.println("\n\nREVISA: ¿Está bien conectada la cinta naranja de la cámara?");
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // Ajustes de imagen para corregir color y orientación
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); 
    s->set_brightness(s, 1); 
    s->set_saturation(s, -2); 
  }
  // Si quieres voltear la imagen, descomenta esto:
  // s->set_vflip(s, 1); 
  // s->set_hmirror(s, 1);

  // ==========================================
  // 5. CONEXIÓN WIFI
  // ==========================================
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Iniciar servidor web
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  
  // Encender LED flash brevemente para indicar éxito (GPIO 4)
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  delay(100);
  digitalWrite(4, LOW);
}

void loop() {
  // El servidor maneja todo, aquí no ponemos nada.
  delay(10000);
}