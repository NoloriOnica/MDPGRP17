from tasks.modules.imgRec import *
from tasks.modules.androidLink import *
from tasks.modules.stm32 import STMLink
from constant.settings import *
import time


### INIT ###

IMG_DIR = "/home/pi/Documents/tasks/capturedImages"
numberOfImagetoCapture = 1
mutex = 0

# Camera start
print("-----------------------------------------")
camera = Camera()
camera.startCamera()
print("Camera Started")

# Stm connect
stm_conn = STMLink()
stm_conn.connect()
print("-----------------------------------------")
print("STM Connected")

# Checklist
print(f"1. Image Reg API Server Started? Is Img Reg Server on {IMGREG_IP}:{IMGREG_PORT}?")
input("Send Any Key to START: ")
print("-----------------------------------------")

###################################################################

### For first obstacle ###
start_time = time.perf_counter()

for _ in range(2):
    time.sleep(1)
    stm_conn.send("APPROACH 25")
    while True:
        recv_msg = stm_conn.recv()
        print(recv_msg)
        if recv_msg.split(" ")[0]== "DONE":
            break
    
# Detect image 
print("--------------------------")
#obs1 = camera.take_Images_and_Process(numberOfImagetoCapture,IMG_DIR,1)
obs1 = "right"
print("Result: ",obs1)

if obs1 == "right": #BOX 1 RIGHT
    cmds = ["RIGHT 45","FORWARD 100","LEFT 45","LEFT 45","FORWARD 100","RIGHT 45"]
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break

else: #BOX 1 LEFT
    cmds = ["LEFT 45","FORWARD 100","RIGHT 45","RIGHT 45","FORWARD 100","LEFT 45"]    
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break

###################################################################

### For 2nd obstacle ###
stm_conn.send("APPROACH 25")
while True:
    recv_msg = stm_conn.recv()
    print(recv_msg)
    if recv_msg.split(" ")[0] == "DONE":
        x = int(float((recv_msg.split("dist=")[1])))
        print("X value: ",x)
        break
time.sleep(1)
stm_conn.send("APPROACH 25")
while True:
    recv_msg = stm_conn.recv()
    print(recv_msg)
    if recv_msg.split(" ")[0] == "DONE":
        break

# Detect 2nd image 
print("--------------------------")
#obs2 = camera.take_Images_and_Process(numberOfImagetoCapture,IMG_DIR,1)
obs2 = "left"
print("Result: ",obs2)

stm_conn.send("APPROACH 30")
while True:
    recv_msg = stm_conn.recv()
    print(recv_msg)
    if recv_msg.split(" ")[0] == "DONE":
        break

# TODO: Change detected img
y = (x + 400)

if obs2 == "right": #BOX 2 RIGHT    
    cmds = ["RIGHT 90","FIRL","LEFT 90","LEFT 90","FIRL","LEFT 90",f"FORWARD {y}","LEFT 45"]    
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" dist=")[0] == "DONE FIRL":
                return_forward_dist = int(float(recv_msg.split("dist=")[1]))
                return_dist = int(return_forward_dist * (2/3))+100
            if recv_msg.split(" ")[0] == "DONE":
                break
    cmds_2 = [f"FORWARD {return_dist}","RIGHT 45","APPROACH 15"]
    for cmd in cmds_2:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break
            

else: #BOX 2 LEFT
    cmds = ["LEFT 90","FIRR","RIGHT 90","RIGHT 90","FIRR","RIGHT 90",f"FORWARD {y}","RIGHT 45"]
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" dist=")[0] == "DONE FIRR":
                return_forward_dist = int(float(recv_msg.split("dist=")[1]))
                return_dist = int(return_forward_dist * (2/3))+100
            if recv_msg.split(" ")[0] == "DONE":
                break
    cmds_2 = [f"FORWARD {return_dist}","LEFT 45","APPROACH 15"]
    for cmd in cmds_2:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break

end_time = time.perf_counter()
elapsed_time = end_time - start_time

print(f"Run time: {elapsed_time:.4f} seconds")

# Manual commands 
enable_manual = input("Execute manual commands (Y/N)?") 
if enable_manual == "Y":
    while True:
        instr = input("Insert CMD (Send 'S' to stop): ")
        instr += "\n"
        
        if instr == 'S':
            stm_conn.disconnect()
            break
        
        stm_conn.send(instr)
        stm_conn.recv()

print("- Ended -")



