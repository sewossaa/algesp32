import websocket
import numpy as np
import PySimpleGUI as sg
from PIL import Image
import json
import io

class EditorMatriz:
    def __init__(self, esp_ip):
        self.esp_ip = esp_ip
        self.ws = None
        self.matriz = np.zeros((120, 160), dtype=np.uint8)
        self.conectar()
    
    def conectar(self):
        try:
            self.ws = websocket.WebSocket()
            self.ws.connect(f"ws://{self.esp_ip}:81")
            print("Conectado al ESP32")
        except Exception as e:
            print(f"Error al conectar: {e}")
    
    def obtener_matriz(self):
        if self.ws:
            cmd = json.dumps({"cmd": "GET_MATRIX"})
            self.ws.send(cmd)
            datos = self.ws.recv()
            if len(datos) > 4:
                w = (datos[0] << 8) | datos[1]
                h = (datos[2] << 8) | datos[3]
                self.matriz = np.frombuffer(datos[4:], dtype=np.uint8).reshape(h, w)
                print("Matriz recibida correctamente")
    
    def mostrar_gui(self):
        sg.theme('Dark')
        
        # Interfaz limpia sin el botón de envío
        layout = [
            [sg.Button('Obtener Matriz'), sg.Button('Guardar PNG'), sg.Button('Limpiar')],
            [sg.Image(key='-IMAGE-', size=(500, 500))], 
            [sg.Text('Control de Brillo:')],
            [sg.Slider((0, 255), default_value=128, key='-BRILLO-', orientation='h', size=(40, 15))],
            [sg.Button('Aplicar Brillo'), sg.Button('Invertir')]
        ]
        
        self.window = sg.Window('Editor de Matriz - ESP32', layout)
        
        while True:
            event, values = self.window.read()
            
            if event == sg.WINDOW_CLOSED:
                break
            elif event == 'Obtener Matriz':
                self.obtener_matriz()
                self.mostrar_imagen()
            elif event == 'Limpiar':
                self.matriz = np.zeros((120, 160), dtype=np.uint8)
                self.mostrar_imagen()
            elif event == 'Guardar PNG':
                Image.fromarray(self.matriz).save('matriz.png')
                sg.popup('Guardado como matriz.png')
            elif event == 'Aplicar Brillo':
                self.matriz = np.clip(self.matriz * (values['-BRILLO-']/128), 0, 255).astype(np.uint8)
                self.mostrar_imagen()
            elif event == 'Invertir':
                self.matriz = 255 - self.matriz
                self.mostrar_imagen()
        
        self.window.close()
    
    def mostrar_imagen(self):
        img = Image.fromarray(self.matriz).resize((500, 500))
        bio = io.BytesIO()
        img.save(bio, format="PNG")
        self.window['-IMAGE-'].update(data=bio.getvalue())

if __name__ == "__main__":
    # Asegúrate de poner aquí la IP que te indica el monitor serial
    editor = EditorMatriz("10.243.180.1") 
    editor.mostrar_gui()
