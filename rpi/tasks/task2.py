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
print(f"Image Reg API Server Started? Is Img Reg Server on {IMGREG_IP}:{IMGREG_PORT}?")
print("Select Choice")
print("1) Min-Min")
print("2) WHACK ONLY")
choice = int(input("Select choice: "))

###################################################################

### For first obstacle ###
start_time = time.perf_counter()

for _ in range(2):
    time.sleep(0.3)
    stm_conn.send("APPROACH 25")
    while True:
        recv_msg = stm_conn.recv()
        print(recv_msg)
        if recv_msg.split(" ")[0]== "DONE":
            break
    
# Detect image 
print("--------------------------")
mutex = 0
distance_travelled_back = 0
while mutex == 0:
    try:
        obs1 = camera.take_Images_and_Process(numberOfImagetoCapture,IMG_DIR,2)
        if obs1 == "left" or "right":
            mutex = 1
    except Exception as e:
        print("Cannot detect image")
        stm_conn.send("BACKWARD 50")
        distance_travelled_back += 50
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break

if distance_travelled_back != 0:
    stm_conn.send("APPROACH 25")
    distance_travelled_back += 50
    while True:
        recv_msg = stm_conn.recv()
        print(recv_msg)
        if recv_msg.split(" ")[0] == "DONE":
            break


#obs1 = "left"
print("Result: ",obs1)

if obs1 == "right": #BOX 1 RIGHT
    if choice == 1:
        cmds = ["RIGHT 45","FORWARD 100","LEFT 45","BACKWARD 30","LEFT 45","FORWARD 50","RIGHT 45"]
    else:
        cmds = ["RIGHT 45","FORWARD 100","LEFT 45","FORWARD 50","LEFT 45","FORWARD 90","RIGHT 45"]
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break
        time.sleep(0.3)

else: #BOX 1 LEFT
    if choice == 1:
        cmds = ["LEFT 45","FORWARD 100","RIGHT 45","BACKWARD 30","RIGHT 45","FORWARD 50","LEFT 45"]   
    else:
        cmds = ["LEFT 45","FORWARD 100","RIGHT 45","FORWARD 50","RIGHT 45","FORWARD 90","LEFT 45"]    
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break
        time.sleep(0.3)

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
time.sleep(0.3)
stm_conn.send("APPROACH 25")
while True:
    recv_msg = stm_conn.recv()
    print(recv_msg)
    if recv_msg.split(" ")[0] == "DONE":
        break

# Detect 2nd image 
print("--------------------------")
mutex = 0
distance_travelled_back = 0
while mutex == 0:
    try:
        obs2 = camera.take_Images_and_Process(numberOfImagetoCapture,IMG_DIR,2)
        if obs2 == "left" or "right":
            mutex = 1
    except Exception as e:
        print("Cannot detect image")
        stm_conn.send("BACKWARD 50")
        distance_travelled_back += 50
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break

if distance_travelled_back != 0:
    stm_conn.send("APPROACH 25")
    distance_travelled_back += 50
    while True:
        recv_msg = stm_conn.recv()
        print(recv_msg)
        if recv_msg.split(" ")[0] == "DONE":
            break
    
#obs2 = "right"
print("Result: ",obs2)

stm_conn.send("APPROACH 30")
while True:
    recv_msg = stm_conn.recv()
    print(recv_msg)
    if recv_msg.split(" ")[0] == "DONE":
        break

# TODO: Change detected img
if choice == 1:
    y = int((x + 400) * 1.5) 
else:
    y = int((x + 600) * 1.5) 

if obs2 == "right": #BOX 2 RIGHT    
    cmds = ["RIGHT 90","FIRL","LEFT 90","LEFT 90","FIRL","LEFT 90",f"FORWARD {y}","LEFT 90"]    
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" dist=")[0] == "DONE FIRL":
                return_forward_dist = int(float(recv_msg.split("dist=")[1]))
                mid_dist = int(return_forward_dist/2)-400
                return_dist = int(return_forward_dist * (2/3))+100
            if recv_msg.split(" ")[0] == "DONE":
                break
        time.sleep(0.3)

    if mid_dist < 0:
        cmds_2 = [f"BACKWARD {abs(mid_dist)}","RIGHT 90","APPROACH 25"]
    else:
        cmds_2 = [f"FORWARD {abs(mid_dist)}","RIGHT 90","APPROACH 25"]
    for cmd in cmds_2:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break
        time.sleep(0.3)
            

else: #BOX 2 LEFT
    cmds = ["LEFT 90","FIRR","RIGHT 90","RIGHT 90","FIRR","RIGHT 90",f"FORWARD {y}","RIGHT 90"]
    for cmd in cmds:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" dist=")[0] == "DONE FIRR":
                return_forward_dist = int(float(recv_msg.split("dist=")[1]))
                mid_dist = int(return_forward_dist/2)-400
                return_dist = int(return_forward_dist * (2/3))+100
            if recv_msg.split(" ")[0] == "DONE":
                break
        time.sleep(0.3)
    if mid_dist < 0:
        cmds_2 = [f"BACKWARD {abs(mid_dist)}","LEFT 90","APPROACH 25"]
    else:
        cmds_2 = [f"FORWARD {abs(mid_dist)}","LEFT 90","APPROACH 25"]
    for cmd in cmds_2:
        print("--------------------------------------")
        stm_conn.send(cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(recv_msg)
            if recv_msg.split(" ")[0] == "DONE":
                break
        time.sleep(0.3)

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



