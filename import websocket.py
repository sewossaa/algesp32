import websocket
import numpy as np
from PIL import Image
import threading

# CAMBIA ESTO por la IP que te muestre el monitor serie de tu ESP32
ESP32_IP = "192.168.1.45"  
URL_WEBSOCKET = f"ws://{"10.243.180.1"}:81"

def on_message(ws, message):
    # Verificar si los datos recibidos son binarios
    if isinstance(message, bytes):
        if len(message) < 4:
            return

        # 1. Reconstruir el ancho y alto desde los primeros 4 bytes
        w = (message[0] << 8) | message[1]
        h = (message[2] << 8) | message[3]
        print(f"¡Imagen recibida! Dimensiones: {w}x{h}")

        # 2. Extraer los píxeles (el resto del mensaje)
        pixel_data = message[4:]

        try:
            # 3. Convertir el buffer a una matriz de numpy (Escala de grises)
            img_array = np.frombuffer(pixel_data, dtype=np.uint8)
            
            # Ajustar la forma de la matriz al tamaño exacto
            img_array = img_array[:w*h].reshape((h, w))

            # 4. Crear la imagen y mostrarla
            img = Image.fromarray(img_array, mode='L')
            
            # Nota: .show() abre el visor de imágenes predeterminado de tu sistema operativo
            img.show() 
            
        except Exception as e:
            print(f"Error al procesar la imagen: {e}")

def on_error(ws, error):
    print(f"Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("Conexión cerrada con el ESP32")

def on_open(ws):
    print("Conectado exitosamente al ESP32. Esperando imágenes del celular...")

if __name__ == "__main__":
    # Configurar el cliente WebSocket
    ws = websocket.WebSocketApp(
        URL_WEBSOCKET,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close
    )

    # Ejecutar en un bucle infinito para mantener la conexión viva
    ws.run_forever()