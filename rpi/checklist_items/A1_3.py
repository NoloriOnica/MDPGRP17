from communication.android import AndroidMessage, AndroidLink
from communication.stm32 import STMLink
from constant.settings import BAUD_RATE, SERIAL_PORT

# Bluetooth portion
android_conn = AndroidLink()
print("Connecting")
android_conn.connect()
print("Connected")
test_msg = AndroidMessage("info", "Messaging works")
print("Message send: ",test_msg._value)
android_conn.send(test_msg)

# STM portion
stm_conn = STMLink()
stm_conn.connect()

while True:
    resp = android_conn.recv()
    print("Input received from Android: ",resp)
    resp = resp.upper()
    
    if resp == 'LEFT' or resp == "RIGHT":
        resp += " 90\n"
    elif resp == "FORWARD" or resp == "BACKWARD":
        resp += " 500\n"
    else:
        resp += "\n"
    
    msgToSTM = AndroidMessage("info", resp)
    print("STM CMD Send from Android: ",msgToSTM._value)
    stm_conn.send(msgToSTM._value)
    stm_conn.recv()


# ser.write(resp.encode())
# print(f'CMD SENT:{resp}')
        
# for _ in range(2):
#     line = ser.readline().decode("utf-8", errors="ignore").strip()
#     if line:
#         print("recv:", line)


# # Pass command to stm
# stm_conn.connect()
# stm_conn.send_cmd(b"FORWARD 500")


