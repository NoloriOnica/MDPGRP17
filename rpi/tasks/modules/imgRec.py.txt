from constant.settings import IMGREG_IP, IMGREG_PORT
import requests
import os
from picamera2 import Picamera2

class Camera:
    def __init__(self):
        self.IMGREC_IP = IMGREG_IP
        self.IMGREC_PORT = IMGREG_PORT       

    def startCamera(self):
        self.picam2 = Picamera2()
        self.picam2.configure(self.picam2.create_video_configuration(main={"size": (1280, 1000)}))
        self.picam2.start()

    def stopCamera(self):
        self.picam2.stop()

    def getResults(self, img_file_path):
        dest = f"http://{self.IMGREC_IP}:{self.IMGREC_PORT}/detect"

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
            self.takeSnapshot(img_name)

            # {'timestamp': '2026-02-26T00:47:41.325468', 'detections': [{'id': 16, 'name': 'F', 'confidence': 0.92}], 'count': 1}
            try:
                json_resp = self.getResults(img_name)
                detected_img = json_resp["detections"][0]["name"]
                confidence = json_resp["detections"][0]["confidence"]
            except Exception:
                print("Cannot detect image")
            print("Detected Image: ", detected_img, "|  Confidence: ", confidence)

            if result_count.get(detected_img) is None:
                result_count[detected_img] = 1
            else: 
                result_count[detected_img] += 1

        final_result = max(result_count,key=result_count.get)
        return final_result