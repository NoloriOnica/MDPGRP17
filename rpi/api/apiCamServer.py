# pip install fastapi uvicorn picamera2 opencv-python requests
# To run: python apiCamServer.py


from fastapi import FastAPI, Response
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, StreamingResponse
import cv2
import numpy as np
from picamera2 import Picamera2
import io
import time
import requests
from datetime import datetime
from constant.settings import IMGREG_IP  # PC: 192.168.17.18
import uvicorn

app = FastAPI(title="RPi Camera Server")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_credentials=True, allow_methods=["*"], allow_headers=["*"])

# Global camera
picam2 = Picamera2()
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
picam2.start()

@app.get("/stream")
async def stream():
    """MJPEG stream"""
    def generate():
        while True:
            frame = picam2.capture_array()
            _, jpeg = cv2.imencode('.jpg', frame)
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + jpeg.tobytes() + b'\r\n')
            time.sleep(0.03)  # ~30fps
    
    return StreamingResponse(generate(), media_type="multipart/x-mixed-replace;boundary=frame")

