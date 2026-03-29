import inspect

from tasks.modules.imgRec import *
from tasks.modules.android import AndroidLink, AndroidMessage
from tasks.modules.androidLink import *
from tasks.modules.algo import *
from tasks.modules.stm32 import STMLink

from multiprocessing import Process, Queue
import time

from constant.settings import ALGO_IP, ALGO_PORT, IMGREG_IP, IMGREG_PORT

# TODO: Get android to send back ACK after receiving image result
# TODO: Bluetooth reconnection (consider storing state) (USED STORING STATE, BLUETOOTH TO MANUALLY RECONNECT - TO TEST IF BLUETOOTH DISCONNECTS)
# TODO: The car continues to move even if no detection (DONE)

### INIT ###

IMG_DIR = "/home/pi/Documents/tasks/capturedImages"
numberOfImagetoCapture = 3
mutex = 0
result_history = {}

# Checklist
# print(f"1. Image Reg API Server Started? Is Img Reg Server on {IMGREG_IP}:{IMGREG_PORT}?")
# print(f"2. Algo API Server Started? Is Algo Server on {ALGO_IP}:{ALGO_PORT}?")
# print("3. Android ready to bluetooth pair?")
# input("Send Any Key When Done: ")
# print("-----------------------------------------")

# Android connect (with reconnection)
android_conn = AndroidLink()
androidSend_Queue = Queue()
androidRecv_Queue = Queue()
obs_queue = Queue()
procs = []
# print(">>> AndroidLink loaded from:", inspect.getfile(AndroidLink))

android_conn.connect()
print("Android Bluetooth Connected")


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

# Getting Obs pos from Android, parse to algo server, receive optimal stm_commands
    # start multi-processes
recv_p = Process(target=continuous_recv, args=(android_conn, androidRecv_Queue))
send_p = Process(target=continuous_send, args=(android_conn, androidSend_Queue))
monitor_p = Process(target=monitor_connection, args=(android_conn, procs, androidRecv_Queue,androidSend_Queue))

recv_p.start()
send_p.start()
monitor_p.start()
procs = [recv_p, send_p, monitor_p]
print("-----------------------------------------")
print("All multiprocesses started")





### Android -> RPI -> Algo -> RPI ###

# Listening for obstacles from Android
print("-----------------------------------------")
print("Waiting for Obstacle Positions from Android")
while True and mutex == 0:
    while not androidRecv_Queue.empty():
        recv_msg = androidRecv_Queue.get()
        print("Received Message from Android:    ", recv_msg)
        
        # Sample android output
        # OBS|{id: 0,x: 7,y: 7,d: N}{id: 1,x: 14,y: 8,d: N}{id: 2,x: 7,y: 12,d: N}{id: 3,x: 15,y: 4,d: N}{id: 4,x: 10,y: 4,d: N}{id: 5,x: 16,y: 11,d: N}{id: 6,x: 4,y: 10,d: N}{id: 7,x: 15,y: 11,d: N}
        
        if recv_msg[0:3] == "OBS":
            ## ACTUAL
            # msg_for_algo = parse_to_algo(recv_msg)
            # resp = getSTMCommands(msg_for_algo)
            # cmds_for_stm = resp["instructions"]
            # obs_seq = resp["visiting"]
            # for obs in obs_seq:
            #     obs_queue.put(obs)
            ## FOR TEST
            cmds_for_stm = ["APPROACH 25","READ","RIGHT 45","LEFT 45","APPROACH 25","READ"] # TO REMOVE
            obs_seq = [1,2]
            # TODO: need get sequence of which blocks are approached first
            print("Optimal STM Commands from Algo Server:")
            print(cmds_for_stm)
            # print("Visiting Sequence")
            # print(obs_seq)
            mutex = 1
            break
    




### READY TO START ###

# Signal ready to start on Android
if cmds_for_stm is not None:             
    print("-----------------------------------------")
    print("READY TO START ON ANDROID!")

while androidRecv_Queue.empty():
    recv_msg = androidRecv_Queue.get()
    if recv_msg == "START_C1":
        print("STARTED! (PRAY IT WORKS)")
        break



### RPI -> STM ###              
## FOR TEST
## obs_queue.put(1)
time.sleep(3)

# Start issuing commands to STM
iter_index = 0

print("-----------------------------------------")
print(f"Moving to Obstacle Number {obs_seq[iter_index]}")

for stm_cmd in cmds_for_stm:
    print(f"Sending command: {stm_cmd}")
    
    # If READ start detecting image
    if stm_cmd.split(" ")[0] == "READ" and iter_index < len(obs_seq):
        print("Detecting Image: ")
        obs_index = obs_seq[iter_index]

        # Detect image
        try:
            detected_img = camera.take_Images_and_Process(numberOfImagetoCapture,IMG_DIR,obs_index)
        except Exception as e:
            print("Unable to detect images, will proceed anyway.")
            iter_index += 1
            print("-----------------------------------------")
            print(f"Moving to Obstacle Number {obs_index}")
            continue

        print("Result: ",detected_img)
        
        # send detection result to android
        print("Sending Result to Android")
        result_msg = AndroidMessage("",f"TARGET-{obs_index}-{detected_img}")
        androidSend_Queue.put(result_msg)
        print(f"Message Sent:", f"TARGET-{obs_index}-{detected_img}")
        result_history[obs_index] = detected_img
        
        # # TO REMOVE IN ACTUAL:
        # androidRecv_Queue.put("ACK")

        # while True:
        #     msg = androidRecv_Queue.get()
        #     # TODO: ask android to send back ACK after getting image
        #     if msg == "ACK":
        #         print("Android received results")
        #         break
        #     print("Message Received from Android: ",msg)

        # Move to next obs
        iter_index += 1
        print("-----------------------------------------")
        print(f"Moving to Obstacle Number {obs_index}")
    
    # Else just issue commands          
    else:
        stm_conn.send(stm_cmd)
        while True:
            recv_msg = stm_conn.recv()
            print(f"From STM: {recv_msg}")
            
            if recv_msg.split(" ")[0] == "DONE":
                print("-")
                break

# Dump the queue    
while not androidRecv_Queue.empty():
    androidRecv_Queue.get()

# If Android is disconnected wait for it to reconnect    
while not android_conn.get_connection_status():
    continue

# Parse the results again back to Android, incase along the way bluetooth disconnected
for obs_index, result in result_history.items():
    result_msg = AndroidMessage("",f"TARGET-{obs_index}-{result}")
    androidSend_Queue.put(result_msg)
    print(f"Message Sent to Android:,"f"TARGET-{obs_index}-{result}")


### Disconnect ###
print("ROBOT TERMINATED! WE R DONE WITH TASK 1!")

# Kill all connection and processes
kill_subProcesses(procs)
android_conn.disconnect()
camera.stopCamera()
stm_conn.disconnect()
print("- Can exit Program Now -")
