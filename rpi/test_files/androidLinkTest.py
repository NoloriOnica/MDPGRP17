from communication.android import AndroidMessage, AndroidLink
from multiprocessing import Process, Queue
import time
import sys
import select
    
def continuous_recv(connection: AndroidLink):
    """Continuously receive from Android and print."""
    while True:
        try:
            msg = connection.recv()          # string from Android
            if msg is not None:
                print("--------------------------------------------------")
                print(f"Message Received: {msg}")
        except Exception as e:
            print(f"[recv] error: {e}")
            break


def continuous_send(connection: AndroidLink, q: Queue):
    """Continuously read AndroidMessage from q and send to Android."""
    while True:
        try:
            msg: AndroidMessage = q.get()    # blocks until main puts something
            if msg is None:
                break                        # shutdown signal
            connection.send(msg)             # your AndroidLink.send(AndroidMessage)
            print("--------------------------------------------------")
            print(f"Message Sent: {msg.value}")
        except Exception as e:
            print(f"[send] error: {e}")
            break
        
def monitor_connection(connection: AndroidLink, procs: list, q: Queue):
    """Dedicated process: Detects disconnect, kills/restarts atomically."""
    while True:
        if not connection.get_connection_status():
            print("--------------------------------------------------")
            print("Connection lost - reconnecting")
            kill_subProcesses(procs)
            q.put(None)
            connection.disconnect()
            time.sleep(3)
            
            # Restart procs (inherits fresh socket via fork)
            new_procs = []
            connection.connect()
            print("Reconnected to Android")
            
            recv_p = Process(target=continuous_recv, args=(connection,))
            send_p = Process(target=continuous_send, args=(connection, q))
            recv_p.start()
            send_p.start()
            new_procs.extend([recv_p, send_p])
            
            # Self-terminate + replace via queue (or use Pipe for procs list)
            # For simplicity: Main restarts monitor too (see below)
        
        time.sleep(0.5)  # Poll interval

def kill_subProcesses(processes):
    for p in processes:
        if p.is_alive():
            p.terminate()
            p.join(timeout=3)

# -----------------------------------------------------#

# Initial start (no monitor race)
android_conn = AndroidLink()
sendQueue = Queue()
procs = []

android_conn.connect()
print("Initial connect")

recv_p = Process(target=continuous_recv, args=(android_conn,))
send_p = Process(target=continuous_send, args=(android_conn, sendQueue))
monitor_p = Process(target=monitor_connection, args=(android_conn, procs, sendQueue))

recv_p.start()
send_p.start()
monitor_p.start()
procs = [recv_p, send_p, monitor_p]  # Include monitor

try:
    while True:
        ready, _, _ = select.select([sys.stdin], [], [], 0.1)
        if ready:
            msg_text = sys.stdin.readline().strip()
            if msg_text:
                msg = AndroidMessage("From RPI", msg_text)
                sendQueue.put(msg)

except KeyboardInterrupt:
    print("Disconnecting")

finally:
    kill_subProcesses(procs)
    android_conn.disconnect()

