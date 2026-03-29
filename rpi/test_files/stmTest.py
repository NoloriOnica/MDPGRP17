from constant.settings import SERIAL_PORT, BAUD_RATE, IMGREG_IP, IMGREG_PORT
from communication.stm32 import STMLink
from tasks.modules.imgRec import *
import time

# 3,6,s
# 3,15,s
# 6,9,w
# 12,15,s
# 15,3,w
# 15,12,s
# 9,18,e
# 12,9,w

# cmd_list = ["RIGHT 36","FORWARD 160","LEFT 12","FORWARD 80","LEFT 24","PAUSE 500","READ 500","BACKLEFT 48","RIGHT 36","FORWARD 320","LEFT 24","FORWARD 200","RIGHT 24","PAUSE 500","READ 500","BACKRIGHT 48","LEFT 84","FORWARD 480","RIGHT 24","FORWARD 80","RIGHT 24","PAUSE 500","READ 500","BACKLEFT 60","RIGHT 108","BACKLEFT 12","PAUSE 500","READ 500","BACKLEFT 36","BACKWARD 80","BACKLEFT 24","BACKWARD 160","BACKLEFT 36","PAUSE 500","READ 500","LEFT 24","BACKRIGHT 72","PAUSE 500","READ 500","RIGHT 24","BACKLEFT 48","BACKWARD 160","BACKLEFT 84","BACKRIGHT 48","FORWARD 80","BACKRIGHT 12","PAUSE 500","READ 500"]

# Santosh
cmd_list = ["RIGHT 48","FORWARD 80","RIGHT 12","FORWARD 360","RIGHT 24","PAUSE 500","READ 500","BACKRIGHT 12","LEFT 72","FORWARD 80","PAUSE 500","READ 500","BACKLEFT 12","RIGHT 24","FORWARD 520","LEFT 36","PAUSE 500","READ 500","BACKRIGHT 12","LEFT 84","PAUSE 500","READ 500","LEFT 24","FORWARD 80","BACKRIGHT 12","FORWARD 240","RIGHT 24","FORWARD 80","LEFT 72","PAUSE 500","READ 500","BACKRIGHT 12","LEFT 84","FORWARD 560","RIGHT 24","FORWARD 120","RIGHT 72","PAUSE 500","READ 500","BACKLEFT 24","RIGHT 12","FORWARD 120","LEFT 12","FORWARD 120","LEFT 12","RIGHT 72","BACKWARD 80","PAUSE 500","READ 500","RIGHT 24","BACKLEFT 60","RIGHT 24","FORWARD 80","RIGHT 12","LEFT 12","RIGHT 24","FORWARD 160","LEFT 36","PAUSE 500","READ 500"]



def main():
    stm_conn = STMLink()
    stm_conn.connect()
    
    # Possible cmds: FORWARD 500, BACKWARD 500, LEFT 90, RIGHT 90, STOP, PAUSE, BACKLEFT 45, APPROACH 25
    while True:
        instr = input("Insert CMD (Send 'S' to stop): ")
        instr += "\n"
        
        if instr == 'S':
            stm_conn.disconnect()
            return
        
        stm_conn.send(instr)
        
        while True:
            recv_msg = stm_conn.recv()
            print(f"From STM: {recv_msg}")
            
            if recv_msg.split(" ")[0] == "DONE":
                print("-")
                break

def hardcoded():
    stm_conn = STMLink()
    stm_conn.connect()
    camera = Camera()
    camera.startCamera()
    obs_index = 0
    IMG_DIR = "/home/pi/Documents/tasks/capturedImages"

    for cmd in cmd_list:

        if cmd.split(" ")[0] != "READ":
            print("Cmd send:",cmd)
            stm_conn.send(cmd)

            while True:
                recv_msg = stm_conn.recv()
                print(f"From STM: {recv_msg}")
                
                if recv_msg.split(" ")[0] == "DONE":
                    print("-")
                    break
        
        else:
            for _ in range(3):
                print("Detecting Image: ")
                try:
                    detected_img = camera.take_Images_and_Process(1,IMG_DIR,obs_index)
                    print("Result: ",detected_img)
                    obs_index += 1
                except Exception as e:
                    print("Unable to detect images, will proceed anyway.")
                    continue
            obs_index += 1

    camera.stopCamera() 

#hardcoded()
main()