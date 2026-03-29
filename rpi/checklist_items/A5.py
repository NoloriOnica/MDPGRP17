from constant.settings import SERIAL_PORT, BAUD_RATE
from communication.stm32 import STMLink
from communication.imgReg import *
import time

cmd_set_1 = ["BACKLEFT 45","RIGHT 45","FORWARD 250","LEFT 90","BACKWARD 60","LEFT 90", "APPROACH 25"]
# ["RIGHT 45","LEFT 45","LEFT 45","BACKWARD 250","BACKRIGHT 50"]

# ["RIGHT 45","LEFT 45","LEFT 45","BACKWARD 250","BACKRIGHT 50"]

# ["RIGHT 90","LEFT 90","BACKWARD 250","LEFT 90"]

# ["BACKLEFT 45","RIGHT 45","FORWARD 250","LEFT 90","BACKWARD 60","LEFT 90", "APPROACH 25"]
IMG_DIR = "/home/pi/Documents/checklist_items"
#test_images = ["/home/pi/Documents/test_files/images/testImg.jpg","/home/pi/Documents/test_files/images/testImg.jpg","/home/pi/Documents/test_files/images/testImg.jpg","/home/pi/Documents/test_files/images/F.jpg"]

stm_conn = STMLink()
stm_conn.connect()
picamObj = startCamera()


#print("APPROACH 25")

for index in range(1,5): # TODO: Change to range(4) after camera fixed  
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
    
        
# while True:
#     instr = input("Insert CMD (Send 'S' to stop): ")
#     instr += "\n"
    
#     if instr == 'S':
#         stm_conn.disconnect()
#         break
    
#     stm_conn.send(instr)
#     stm_conn.recv()