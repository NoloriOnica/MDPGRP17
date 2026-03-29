from constant.settings import SERIAL_PORT, BAUD_RATE
from communication.stm32 import STMLink
from communication.imgReg import *
import time

cmd_set_1 = ["BACKLEFT 45","RIGHT 45","FORWARD 250","LEFT 90","BACKWARD 60","LEFT 90", "APPROACH 25"]

IMG_DIR = "/home/pi/Documents/checklist_items"

stm_conn = STMLink()
stm_conn.connect()
picamObj = startCamera()


for index in range(1,5): 
    stm_conn.send("APPROACH 25")
    while True:
        recv_msg = stm_conn.recv()
        print(recv_msg)
        if recv_msg == "DONE APPROACH":
            break
        
    # capture 5 images 
    print("--------------------------")
    detected_img = take_Images_and_Process(picamObj,3,IMG_DIR,index)
    print("Result: ",detected_img)
    # {'timestamp': '2026-02-26T00:47:41.325468', 'detections': [{'id': 16, 'name': 'F', 'confidence': 0.92}], 'count': 1}
    if detected_img != "Bullseye":
        print("ROBOT TERMINATED")
        break
    
    else:
        for cmd in cmd_set_1:
            print("--------------------------")
            #print(cmd)
            stm_conn.send(cmd)
            while True:
                recv_msg = stm_conn.recv()

                print(recv_msg)
                if recv_msg.split(" ")[0] == "DONE":
                    break