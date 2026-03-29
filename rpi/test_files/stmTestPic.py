from tasks.modules.imgRec import *
from constant.settings import SERIAL_PORT, BAUD_RATE, IMGREG_IP, IMGREG_PORT
from communication.stm32 import STMLink


IMG_DIR = "/home/pi/Documents/tasks/capturedImages/today"


camera = Camera()
camera.startCamera()


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
        
        if instr == "SNAP\n":
            try:
                camera.take_Images_and_Process(3, IMG_DIR, 1)
            except Exception as e:
                print("Unable to detect images, will proceed anyway.")
                continue
        
        stm_conn.send(instr)
        
        while True:
            recv_msg = stm_conn.recv()
            print(f"From STM: {recv_msg}")
            
            if recv_msg.split(" ")[0] == "DONE":
                print("-")
                break


cmd_list = ["FORWARD 700","BACKRIGHT 90","BACKWARD 200","BACKRIGHT 90","SNAP","RIGHT 90","LEFT 90","BACKWARD 300","BACKLEFT 90","BACKLEFT 90","BACKWARD 50","SNAP","FORWARD 50","LEFT 90","RIGHT 90","FORWARD 200","RIGHT 90","FORWARD 250","BACKLEFT 90","FORWARD 50","SNAP","LEFT 90","FORWARD 600","SNAP","BACKRIGHT 90","FORWARD 200","SNAP","FORWARD 100","BACKRIGHT 90","FORWARD 150","SNAP","BACKLEFT 90","BACKWARD 150","BACKLEFT 90","FORWARD 50","SNAP","BACKWARD 50","LEFT 90","FORWARD 450","BACKLEFT 90","FORWARD 50","SNAP"]
def hardcoded():
    print("hello")
    stm_conn = STMLink()
    stm_conn.connect()

    obs_index = 0

    for instr in cmd_list:

        instr += "\n"
        
        if instr == 'S':
            stm_conn.disconnect()
            return
        
        if instr == "SNAP\n":
            try:
                camera.take_Images_and_Process(3, IMG_DIR, 1)
            except Exception as e:
                print("Unable to detect images, will proceed anyway.")
            continue
        
        stm_conn.send(instr)
        
        while True:
            recv_msg = stm_conn.recv()
            print(f"From STM: {recv_msg}")
            
            if recv_msg.split(" ")[0] == "DONE":
                print("-")
                break

    camera.stopCamera() 


# hardcoded()

main()