import requests
import os
from constant.settings import IMGREG_IP
from picamera2 import Picamera2

def startCamera():
    picam2 = Picamera2()
    picam2.configure(picam2.create_video_configuration(main={"size": (1000, 1000)}))
    picam2.start()
    
    return picam2

def stopCamera(picamObj: Picamera2):
    picamObj.stop()

def getResults(img_file_path):
    dest = f"http://{IMGREG_IP}:8000/detect"

    with open(img_file_path, 'rb') as f:
        files = {'file': f}
        response = requests.post(dest, files=files, timeout=10)

    return response.json()

def takeSnapshot(picamObj: Picamera2, filepath):
    picamObj.capture_file(filepath)
    print(f"Saved snapshot to {filepath}")
    
def take_Images_and_Process(picamObj: Picamera2, noOfImagesToCapture, imgDirectory, objIndex):
    result_count = {}
    
    for index in range(noOfImagesToCapture):
        img_name = imgDirectory + "/" + f"{objIndex}_" + f"{index}.jpg"
        takeSnapshot(picamObj,img_name)
        
        json_resp = getResults(img_name)
        detected_img = json_resp["detections"][0]["name"]
        confidence = json_resp["detections"][0]["confidence"]
        print("Detected Image: ", detected_img, "|  Confidence: ", confidence)
        
        if result_count.get(detected_img) is None:
            result_count[detected_img] = 1
        else: 
            result_count[detected_img] += 1
    
    final_result = max(result_count,key=result_count.get)
    return final_result 


# url = "http://192.168.17.30:7123/detect"
# CAPTURE_DIR = "/home/pi/Documents/test_files/images"
# IMG_NAME = "curImg.jpg"
# IMG_PATH = os.path.join(CAPTURE_DIR, IMG_NAME)

# # Ensure folder exists
# os.makedirs(CAPTURE_DIR, exist_ok=True)

# Initialize camera
# picam2 = Picamera2()
# picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
# picam2.start()  # start the camera

# def takeSnapshot(filepath):
#     picam2.capture_file(filepath)
#     print(f"Saved snapshot to {filepath}")

# def sendSnapshot(filepath):
#     with open(filepath, "rb") as f:
#         img_bytes = f.read()
#     try:
#         response = requests.post(url, data=img_bytes, timeout=5)
#         print("Server response:", response.json())
#         return response
#     except requests.RequestException as e:
#         print("Error sending snapshot:", e)
#         return None

# # Take a snapshot and send it
# takeSnapshot(IMG_PATH)
# sendSnapshot(IMG_PATH)

# picam2.stop()