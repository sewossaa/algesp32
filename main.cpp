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
    // --- NUEVO: Crear un string limpio ignorando espacios o saltos de línea al final ---
    String cleanPayload = String(payload);
    cleanPayload.trim(); // Esto borra espacios, \n y \r que meten las apps de celular

    JsonDocument doc;
    // Usamos cleanPayload.c_str() en lugar del payload original
    DeserializationError error = deserializeJson(doc, cleanPayload.c_str());

    if (error)
    {
      Serial.print("Error JSON: ");
      Serial.println(error.c_str()); // Modificado para que te diga el motivo exacto del error
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

  void processImageData(char *payload, size_t length)
  {
    if (length < 4)
      return;

    // Primeros 4 bytes: ancho y alto
    int w = ((uint8_t)payload[0] << 8) | (uint8_t)payload[1];
    int h = ((uint8_t)payload[2] << 8) | (uint8_t)payload[3];

    Serial.printf("Recibiendo imagen: %dx%d (%d bytes)\n", w, h, length);

    // Copiar datos a la matriz local del ESP32
    int copySize = min((int)(length - 4), MATRIX_WIDTH * MATRIX_HEIGHT);
    memcpy(imageMatrix, &payload[4], copySize);

    Serial.println("Imagen almacenada de forma local");

    // === NUEVO: Retransmitir la imagen a los demás clientes (Python) ===
    webSocket.broadcastBIN((uint8_t *)payload, length);
    Serial.println("Imagen retransmitida a la PC");
  }

  void sendMatrixToClient(uint8_t num) 
  {
    // Al agregar 'static', el buffer no satura la pila (Stack)
    static uint8_t buffer[4 + MATRIX_WIDTH * MATRIX_HEIGHT];

    buffer[0] = MATRIX_WIDTH >> 8;
    buffer[1] = MATRIX_WIDTH & 0xFF;
    buffer[2] = MATRIX_HEIGHT >> 8;
    buffer[3] = MATRIX_HEIGHT & 0xFF;

    memcpy(&buffer[4], imageMatrix, MATRIX_WIDTH * MATRIX_HEIGHT);

    webSocket.sendBIN(num, buffer, sizeof(buffer));
    Serial.println("Matriz enviada");
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
    Serial.begin(9600);

    Serial.println("\n\n=== ESP32 WebSocket Server ===");

    // Inicializar matriz (prueba con patrón)
    memset(imageMatrix, 0, sizeof(imageMatrix));

    // Conectar a WiFi
    Serial.print("Conectando a WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_AP_STA);
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