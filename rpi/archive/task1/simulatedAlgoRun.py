from constant.settings import SERIAL_PORT, BAUD_RATE
from communication.stm32 import STMLink
from communication.imgReg import *
import time

cmd_set_1 = ['FORWARD 270', 'BACKRIGHT 90', 'BACKWARD 350', 'BACKRIGHT 90', 'FORWARD 150', 'BACKLEFT 90', 'FORWARD 260', 'APPROACH 20', 'SNAP 1_C', 'BACKRIGHT 90', 'FORWARD 210', 'BACKRIGHT 90', 'FORWARD 60', 'BACKLEFT 90', 'BACKWARD 190', 'BACKLEFT 90', 'FORWARD 70', 'BACKRIGHT 90', 'FORWARD 190', 'APPROACH 20', 'SNAP 2_C']
             
IMG_DIR = "/home/pi/Documents/checklist_items"
#test_images = ["/home/pi/Documents/test_files/images/testImg.jpg","/home/pi/Documents/test_files/images/testImg.jpg","/home/pi/Documents/test_files/images/testImg.jpg","/home/pi/Documents/test_files/images/F.jpg"]

stm_conn = STMLink()
stm_conn.connect()
# picamObj = startCamera()


#print("APPROACH 25")

# for index in range(1,5): # TODO: Change to range(4) after camera fixed  
#     stm_conn.send("APPROACH 25")
#     while True:
#         recv_msg = stm_conn.recv()
#         print(recv_msg)
#         if recv_msg == "DONE APPROACH":
#             break
        
#     # capture 5 images 
#     print("--------------------------")
#     detected_img = take_Images_and_Process(picamObj,5,IMG_DIR,index)
#     print("Result: ",detected_img)
#     # {'timestamp': '2026-02-26T00:47:41.325468', 'detections': [{'id': 16, 'name': 'F', 'confidence': 0.92}], 'count': 1}
#     if detected_img != "Bullseye":
#         print("ROBOT TERMINATED")
#         break
    
#     else:
for cmd in cmd_set_1:
    print("--------------------------")
    #print(cmd)
    if cmd.split(" ")[0] == "FORWARD" or cmd.split(" ")[0] == "BACKWARD":
        cmd = f'{cmd.split(" ")[0]} {float(cmd.split(" ")[1])}'
        stm_conn.send(cmd)
        print(cmd)
    elif cmd.split(" ")[0] == "SNAP":
        print(cmd)
    else:
        stm_conn.send(cmd)
        print(cmd)
    while True:
        recv_msg = stm_conn.recv()

        print(recv_msg)
        if recv_msg.split(" ")[0] == "DONE":
            break