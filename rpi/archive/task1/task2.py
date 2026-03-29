from constant.settings import SERIAL_PORT, BAUD_RATE
from communication.stm32 import STMLink
from communication.imgReg import *
import time

cmd_set_1 = ["APPROACH 20","BACKWARD 100","RIGHT 90","BACKWARD 100","LEFT 90","FORWARD 100","LEFT 90","BACKWARD 100","RIGHT 90","APPROACH 20","LEFT 90"]

IMG_DIR = "/home/pi/Documents/checklist_items"


stm_conn = STMLink()
stm_conn.connect()
picamObj = startCamera()

stm_conn.send("APPROACH 20")
while True:
    recv_msg = stm_conn.recv()
    print(recv_msg)
    if recv_msg == "DONE APPROACH":
        break
    
# capture 5 images 
print("--------------------------")
detected_img = take_Images_and_Process(picamObj,3,IMG_DIR,1)
print("Result: ",detected_img)
# {'timestamp': '2026-02-26T00:47:41.325468', 'detections': [{'id': 16, 'name': 'F', 'confidence': 0.92}], 'count': 1}
if detected_img == "Right":
    cmds = ["BACKWARD 100","RIGHT 90","BACKWARD 100","LEFT 90","FORWARD 100","LEFT 90","BACKWARD 100","RIGHT 90","APPROACH 20"]
    for cmd in cmds:
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break
elif detected_img == "Left":
    print("good")
    cmds = ["BACKWARD 100","LEFT 90","BACKWARD 100","RIGHT 90","FORWARD 100","RIGHT 90","BACKWARD 100","LEFT 90","APPROACH 20"]
    for cmd in cmds:
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break

stm_conn.send("APPROACH 20")

        
# while True:
#     instr = input("Insert CMD (Send 'S' to stop): ")
#     instr += "\n"
    
#     if instr == 'S':
#         stm_conn.disconnect()
#         break
    
#     stm_conn.send(instr)
#     stm_conn.recv()



