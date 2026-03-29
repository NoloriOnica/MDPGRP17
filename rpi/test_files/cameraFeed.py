import os
from datetime import datetime
import io
import logging
import socketserver
import json
from http import server
from threading import Condition
import requests

from picamera2 import Picamera2
from picamera2.encoders import JpegEncoder
from picamera2.outputs import FileOutput
from constant.settings import IMGREG_IP 

CAPTURE_DIR = "/home/pi/Documents/PiCameraTest/TestData"


class StreamingOutput(io.BufferedIOBase):
    def __init__(self):
        self.frame = None
        self.condition = Condition()

    def write(self, buf):
        with self.condition:
            self.frame = buf
            self.condition.notify_all()

class StreamingHandler(server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(301)
            self.send_header('Location', '/index.html')
            self.end_headers()
        elif self.path == '/index.html':
            try:
                with open("index.html") as f:
                    content = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'text/html')
                self.send_header('Content-Length', len(content))
                self.end_headers()
                self.wfile.write(content)
            except:
                self.send_error(404, "index.html not found")
        elif self.path == '/stream.mjpg':
            self.send_response(200)
            self.send_header('Age', 0)
            self.send_header('Cache-Control', 'no-cache, private')
            self.send_header('Pragma', 'no-cache')
            self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=FRAME')
            self.end_headers()
            try:
                while True:
                    with output.condition:
                        output.condition.wait()
                        frame = output.frame
                    self.wfile.write(b'--FRAME\r\n')
                    self.send_header('Content-Type', 'image/jpeg')
                    self.send_header('Content-Length', len(frame))
                    self.end_headers()
                    self.wfile.write(frame)
                    self.wfile.write(b'\r\n')
            except Exception as e:
                logging.warning(
                    'Removed streaming client %s: %s',
                    self.client_address, str(e))
        else:
            self.send_error(404)
            self.end_headers()
    def do_POST(self):
        if self.path == '/capture':
            filename = datetime.now().strftime("image_%Y%m%d_%H%M%S.jpg")
            filepath = os.path.join(CAPTURE_DIR, filename)

            picam2.capture_file(filepath)

            self.send_response(200)
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()
            self.wfile.write(f"Saved {filename}".encode())

        # elif self.path == '/label':
        #     content_length = int(self.headers.get('Content-Length', 0))
        #     body = self.rfile.read(content_length)

        #     try:
        #         data = json.loads(body)
        #         label = data.get("label")
        #         print("Received label:", label)

        #         self.send_response(200)
        #         self.end_headers()

        #     except json.JSONDecodeError:
        #         self.send_error(400, "Invalid JSON")
                
        elif self.path == '/label':
        # POST to local server
            try:
                resp = requests.post(f"http://{IMGREG_IP}:5000/label", 
                                    json={"timestamp": "now", "source": "rpi"})
                data = resp.json()
                detections = data.get("detections", [])
                print("YOLO Results:", detections)
                # Send to Android via queue: AndroidMessage("image-rec", {"obstacle_id": "A", "target_id": detections["id"] if detections else None})
            except Exception as e:
                print(f"YOLO API error: {e}")

        else:
            self.send_error(404)

class StreamingServer(socketserver.ThreadingMixIn, server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


picam2 = Picamera2()
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
output = StreamingOutput()
picam2.start_recording(JpegEncoder(), FileOutput(output))

try:
    address = ('', 7123)
    server = StreamingServer(address, StreamingHandler)
    server.serve_forever()
finally:
    picam2.stop_recording()