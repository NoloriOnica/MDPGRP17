import requests
import os
from picamera2 import Picamera2

url = "http://192.168.17.30:7123/detect"
CAPTURE_DIR = "/home/pi/Documents/test_files/images"
IMG_NAME = "curImg.jpg"
IMG_PATH = os.path.join(CAPTURE_DIR, IMG_NAME)

# Ensure folder exists
os.makedirs(CAPTURE_DIR, exist_ok=True)

# Initialize camera
picam2 = Picamera2()
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
picam2.start()  # start the camera

def takeSnapshot(filepath):
    picam2.capture_file(filepath)
    print(f"Saved snapshot to {filepath}")

def sendSnapshot(filepath):
    with open(filepath, "rb") as f:
        img_bytes = f.read()
    try:
        response = requests.post(url, data=img_bytes, timeout=5)
        print("Server response:", response.json())
        return response
    except requests.RequestException as e:
        print("Error sending snapshot:", e)
        return None

# Take a snapshot and send it
takeSnapshot(IMG_PATH)
sendSnapshot(IMG_PATH)

picam2.stop()