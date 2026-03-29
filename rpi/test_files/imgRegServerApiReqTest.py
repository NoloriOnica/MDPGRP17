import requests
import os
from communication.imgReg import *

# Test for send /detect to IMGREG API server
# filename_1 = "/home/pi/Documents/test_files/images/testImg.jpg"
# filename_2 = "/home/pi/Documents/test_files/images/F.jpg"

# result1 = getResults(filename_1)
# result2 = getResults(filename_2)
# print(result1)
# print(result2)

# Test for capture
CAPTURE_DIR = "/home/pi/Documents/test_files/images"
IMG_NAME = "curImg.jpg"
IMG_PATH = os.path.join(CAPTURE_DIR, IMG_NAME)

picamObj = startCamera()
take_Images_and_Process(picamObj,3,CAPTURE_DIR,1)
stopCamera(picamObj)

