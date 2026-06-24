#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ===== CONFIGURACIÓN WiFi =====
const char *ssid = "sewo";         // Nombre de tu red WiFi
const char *password = "soypobre"; // Contraseña WiFi

// ===== CONFIGURACIÓN SERVIDOR =====
WebSocketsServer webSocket = WebSocketsServer(81); // Puerto 81

// ===== MATRIZ DE IMAGEN =====
#define MATRIX_WIDTH 160
#define MATRIX_HEIGHT 120
uint8_t imageMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];

// ===== FORWARD DECLARATIONS (PROTOTIPOS) =====
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void handleCommand(uint8_t num, char *payload, size_t length);
void processImageData(char *payload, size_t length);
void sendMatrixToClient(uint8_t num);
void printWiFiStatus();

// ===== IMPLEMENTACIÓN =====

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Conectado desde %d.%d.%d.%d\n",
                  num, ip[0], ip[1], ip[2], ip[3]);
    break;
  }

  case WStype_DISCONNECTED:
    Serial.printf("[%u] Desconectado\n", num);
    break;

  case WStype_TEXT:
    handleCommand(num, (char *)payload, length);
    break;

  case WStype_BIN:
    processImageData((char *)payload, length);
    break;
  }
}

void handleCommand(uint8_t num, char *payload, size_t length)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error)
  {
    Serial.println("Error JSON");
    return;
  }

  String cmd = doc["cmd"];
  Serial.println("Comando: " + cmd);

  if (cmd == "GET_MATRIX")
  {
    sendMatrixToClient(num);
  }
  else if (cmd == "SET_PIXEL")
  {
    int x = doc["x"];
    int y = doc["y"];
    int val = doc["val"];

    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT)
    {
      imageMatrix[y][x] = val;
      Serial.printf("Píxel [%d,%d] = %d\n", x, y, val);
    }
  }
  else if (cmd == "CLEAR")
  {
    memset(imageMatrix, 0, sizeof(imageMatrix));
    Serial.println("Matriz limpiada");
  }
}

// Variable global para llevar el conteo de los paquetes
int bytesRecibidos = 0;

void processImageData(char *payload, size_t length) {
  // Si nos llegan exactamente 4 bytes, es el encabezado de dimensiones
  if (length == 4) {
    int w = ((uint8_t)payload[0] << 8) | (uint8_t)payload[1];
    int h = ((uint8_t)payload[2] << 8) | (uint8_t)payload[3];
    Serial.printf("Iniciando recepcion: %dx%d\n", w, h);
    bytesRecibidos = 0; // Reseteamos el contador para una foto nueva
    return;
  }

  // Si es un paquete de la imagen, lo copiamos a su lugar en la matriz
  uint8_t* ptr = (uint8_t*)imageMatrix;
  if (bytesRecibidos + length <= MATRIX_WIDTH * MATRIX_HEIGHT) {
    memcpy(ptr + bytesRecibidos, payload, length);
    bytesRecibidos += length;
  }

  // Verificamos si ya completamos los 19,200 bytes
  if (bytesRecibidos == MATRIX_WIDTH * MATRIX_HEIGHT) {
    Serial.println("✓ Imagen completada y guardada en la matriz");
  }
}

void sendMatrixToClient(uint8_t num)
{
  // 1. Calcular el tamaño total necesario
  size_t bufferSize = 4 + (MATRIX_WIDTH * MATRIX_HEIGHT);

  // 2. Pedir memoria en el Heap (malloc)
  uint8_t *buffer = (uint8_t *)malloc(bufferSize);

  // 3. Verificar si el ESP32 nos dio la memoria correctamente
  if (buffer == NULL)
  {
    Serial.println("Error: Memoria RAM insuficiente en el ESP32");
    return;
  }

  // 4. Llenar los datos
  buffer[0] = MATRIX_WIDTH >> 8;
  buffer[1] = MATRIX_WIDTH & 0xFF;
  buffer[2] = MATRIX_HEIGHT >> 8;
  buffer[3] = MATRIX_HEIGHT & 0xFF;

  memcpy(&buffer[4], imageMatrix, MATRIX_WIDTH * MATRIX_HEIGHT);

  // 5. Enviar el buffer
  webSocket.sendBIN(num, buffer, bufferSize);
  Serial.println("Matriz enviada");

  // 6. ¡MUY IMPORTANTE! Liberar la memoria para que no se llene el ESP32
  free(buffer);
}

void printWiFiStatus()
{
  Serial.println("\n=== ESTADO WiFi ===");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Señal: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.println("===================\n");
}

// ===== SETUP =====
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=== ESP32 WebSocket Server ===");

  // Inicializar matriz (prueba con patrón)
  memset(imageMatrix, 0, sizeof(imageMatrix));

  // Conectar a WiFi
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20)
  {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✓ Conectado");
    printWiFiStatus();
  }
  else
  {
    Serial.println("\n✗ No se pudo conectar");
    Serial.println("Revisa SSID y PASSWORD");
  }

  // Iniciar WebSocket Server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("WebSocket Server iniciado en puerto 81");
  Serial.print("Accede desde: ws://");
  Serial.print(WiFi.localIP());
  Serial.println(":81");
}

// ===== LOOP =====
void loop()
{
  webSocket.loop();
  delay(1);
}
