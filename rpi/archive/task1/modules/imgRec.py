import requests
import os
from picamera2 import Picamera2

class Camera:
    def __init__(self):
        self.IMGREC_IP = "192.168.17.30"        

    def startCamera(self):
        self.picam2 = Picamera2()
        self.picam2.configure(self.picam2.create_video_configuration(main={"size": (640, 640)}))
        self.picam2.start()

    def stopCamera(self):
        self.picam2.stop()

    def getResults(self, img_file_path):
        dest = f"http://{self.IMGREC_IP}:8000/detect"

        with open(img_file_path, 'rb') as f:
            files = {'file': f}
            response = requests.post(dest, files=files, timeout=10)

        return response.json()

    def takeSnapshot(self, filepath):
        self.picam2.capture_file(filepath)
        print(f"Saved snapshot to {filepath}")

    def take_Images_and_Process(self, noOfImagesToCapture, imgDirectory, objIndex):
        result_count = {}

        for index in range(noOfImagesToCapture):
            img_name = imgDirectory + "/" + f"{objIndex}_" + f"{index}.jpg"
            self.takeSnapshot(self.picam2,img_name)

            json_resp = self.getResults(img_name)
            detected_img = json_resp["detections"][0]["name"]
            confidence = json_resp["detections"][0]["confidence"]
            print("Detected Image: ", detected_img, "|  Confidence: ", confidence)

            if result_count.get(detected_img) is None:
                result_count[detected_img] = 1
            else: 
                result_count[detected_img] += 1

        final_result = max(result_count,key=result_count.get)
        return final_result