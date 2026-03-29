from picamera2 import Picamera2
import time
from time import sleep
import os

# Test photo directory
TEST_DIR = "/home/pi/PicameraTest"
os.makedirs(TEST_DIR, exist_ok=True)

# Initialize camera
camera = Picamera2()

# Configure for still photo (high res)
camera.configure(camera.create_still_configuration(main={"size": (1920, 1080)}))

print("📸 Starting camera...")
camera.start()
sleep(2)  # Warmup

print("🖼️  Capturing photo...")
filename = f"{TEST_DIR}/test_photo_{int(time.time())}.jpg"
camera.capture_file(filename)

print(f"✅ Photo saved: {filename}")

camera.stop()
print("Camera stopped.")
