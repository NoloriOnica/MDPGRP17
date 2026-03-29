from task1.modules.imgRec import Camera
from task1.modules.android import AndroidLink, AndroidMessage
from task1.modules.androidLink import *

from constant.settings import SERIAL_PORT, BAUD_RATE
from communication.stm32 import STMLink

from multiprocessing import Process, Queue
import time
import sys
import select

# init things
camera = Camera()
camera.startCamera()

time.sleep(5)
camera.stopCamera()

########################################################

# Stm connect
stm_conn = STMLink()
stm_conn.connect()

# Android connect (with reconnection)
android_conn = AndroidLink()
androidSend_Queue = Queue()
androidRecv_Queue = Queue()
procs = []

android_conn.connect()
print("Android Bluetooth Connected")

# Listen for OBS from Android (use multiprocessing queue) (check if can send only final OBS)

    # start multi-processes
recv_p = Process(target=continuous_recv, args=(android_conn,))
send_p = Process(target=continuous_send, args=(android_conn, androidSend_Queue))
monitor_p = Process(target=monitor_connection, args=(android_conn, procs, androidSend_Queue))

recv_p.start()
send_p.start()
monitor_p.start()
procs = [recv_p, send_p, monitor_p]

# Reformat and parse content to Algo API server



# Get response (commands) (try, except)
# Only start executing (1) commands is not None (2) Android pressed start
# Commands starts executing 1 by 1 (use existing A5 checklist code)
# If the command is "Approach" or equivalent, start capturing photo (need to confirm with algo and STM when to start capturing)
# (use ImgReg.py existing functions) for each photo send to ImgReg API server to process and get the mode of detection result
# send the result back to android (TARGET-BOX ID-RESULT)
    # test message 
result = "A"
obs_index = 1
result_msg = AndroidMessage("RESULTS",f"TARGET-{obs_index}-{result}")
android_conn.send(result_msg)

kill_subProcesses(procs)
android_conn.disconnect()

# print run finished
# disconnect android and stm
