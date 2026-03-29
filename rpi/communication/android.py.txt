import json
import logging
import os
import socket
from typing import Dict, Optional, Union
import time

import bluetooth
from .link import Link


logger = logging.getLogger(__name__)


class AndroidMessage:
    """
    Android message sent over Bluetooth connection.
    """
    def __init__(self, cat: str, value: Union[str, Dict[str, int]]) -> None:
        self._cat = cat
        self._value = value

    @property
    def category(self) -> str:
        """
        Returns the message category.
        :return: String representation of the message category.
        """
        return self._cat

    @property
    def value(self) -> str:
        """
        Returns the message as a string.
        :return: String representation of the message.
        """
        if isinstance(self._value, dict):
            raise ValueError("Value is a dictionary, use jsonify instead.")
        return self._value

    @property
    def jsonify(self) -> str:
        """
        Returns the message as a JSON string.
        :return: JSON string representation of the message.
        """
        return json.dumps({"cat": self._cat, "value": self._value})

    def to_string(self) -> str:
        """_summary_

        self.android_queue.put(AndroidMessage("info", "You are reconnected!"))
        -> "info, you are reconnected!"


        self.android_queue.put(
            AndroidMessage(
                "location",
                {
                    "x": cur_location["x"],
                    "y": cur_location["y"],
                    "d": cur_location["d"],
                },
            )
        )
        -> "location:x,y,d"
        """
        if isinstance(self._value, dict):
            # return values in the dictionary as string
            return f"{self._cat};{';'.join([str(v) for v in self._value.values()])}"
        return f"{self._cat};{self._value}"


class AndroidLink(Link):
    """Class for communicating with Android tablet over Bluetooth connection.

    ## General Format
    Messages between the Android app and RPi will be in the following format:
    ```json
    {"cat": "xxx", "value": "xxx"}
    ```

    The `cat` (for category) field with the following possible values:
    - `info`: general messages
    - `error`: error messages, usually in response of an invalid action
    - `location`: the current location of the robot (in Path mode)
    - `image-rec`: image recognition results
    - `mode`: the current mode of the robot (`manual` or `path`)
    - `status`: status updates of the robot (`running` or `finished`)
    - `obstacle`: list of obstacles

    ## Android to RPi

    #### Set Obstacles
    The contents of `obstacles` together with the configured turning radius
    (`settings.py`) will be passed to the Algorithm API.

    ```json
    {
        "cat": "obstacles",
        "value": {
            "obstacles": [{"x": 5, "y": 10, "id": 1, "d": 2}, {"x": 5, "y": 10, "id": 1, "d": 2}],
            "mode": "0",
            "robot_x": 5, 
            "robot_y": 10,
            "robot_dir": 2
        }
    }
    ```
    RPi will store the received commands and path and make a call to the Algorithm API

    ### RPi to STM

    ####  Start
    Signals to the robot to start dispatching the commands (when obstacles are set).
    ```json
    {"cat": "control", "value": "start"}
    ```

    If there are no commands in the queue, the RPi will respond with an error:
    ```json
    {"cat": "error", "value": "Command queue is empty, did you set obstacles?"}
    ```

    ### RPi to Android

    #### Image Recognition

    The obstacle set by NTU
    The target id is what we identify

    ```json
    {"cat": "image-rec", "value": {"obstacle_id": "A", "target_id":  "1"}}
    ```

    #### Location Updates

    In Path mode, the robot will periodically notify Android with the updated location of the robot.
    ```json
    {"cat": "location", "value": {"x": 1, "y": 1, "d": 0}}
    ```
    where `x`, `y` is the location of the robot, and `d` is its direction.
    the direction for `d` being defined as
    ```
        NORTH = 0
        EAST = 2
        SOUTH = 4
        WEST = 6
        SKIP = 8
    ```
    """

    def __init__(self) -> None:
        """
        Initialize the Bluetooth connection.
        """
        super().__init__()
        self.client_sock = None
        self.server_sock = None
        self.connection_status = 0
        
    # def connect(self) -> None:
    #     print("Bluetooth connection started")
    #     try:
    #         os.system("sudo chmod o+rw /var/run/sdp")
    #         os.system("sudo hciconfig hci0 piscan")  # Page and Inquiry scan
    #         os.system("sudo hciconfig hci0 sspmode 1")  # Enable SSP for modern Android []
    #         print("Bluetooth device set to discoverable")
            
    #         self.server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
    #         self.server_sock.bind(("", bluetooth.PORT_ANY))
    #         self.server_sock.listen(1)
    #         self.server_sock.settimeout(1.0)  # Short timeout to avoid infinite hang []

    #         port = self.server_sock.getsockname()[1]
    #         uuid = "94f39d29-7d6d-437d-973b-fba39e49d4ee"

    #         bluetooth.advertise_service(
    #             self.server_sock,
    #             "RobotServer",  # Give it a name []
    #             service_id=uuid,
    #             service_classes=[uuid, bluetooth.SERIAL_PORT_CLASS],
    #             profiles=[bluetooth.SERIAL_PORT_PROFILE],
    #         )

    #         print(f"Awaiting Bluetooth connection on RFCOMM CHANNEL {port}")
    #         self.client_sock, client_info = self.server_sock.accept()
    #         print(f"Accepted connection from: {client_info}")
    #         self.server_sock.settimeout(None)  # Reset timeout after connect

    #     except bluetooth.BluetoothTimeoutError:
    #         print("Bluetooth accept timeout - check pairing and discoverability")
    #         self._cleanup_sockets()
    #         raise
    #     except Exception as e:
    #         print(f"Error in Bluetooth link connection: {e}")
    #         self._cleanup_sockets()
    #         raise

    def connect(self, max_retries: int = 5, retry_delay: float = 5.0) -> None:
        print("Bluetooth connection started")
        for attempt in range(max_retries):
            try:
                os.system("sudo chmod o+rw /var/run/sdp")
                os.system("sudo hciconfig hci0 piscan")
                os.system("sudo hciconfig hci0 sspmode 1")
                print("Bluetooth device set to discoverable")
                
                self.server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
                self.server_sock.bind(("", bluetooth.PORT_ANY))
                self.server_sock.listen(1)
                self.server_sock.settimeout(30.0)  # Longer initial timeout [web:22]

                port = self.server_sock.getsockname()[1]
                uuid = "94f39d29-7d6d-437d-973b-fba39e49d4ee"
                service_name = "RobotSerial"

                bluetooth.advertise_service(
                    self.server_sock, service_name,
                    service_id=uuid,
                    service_classes=[uuid, bluetooth.SERIAL_PORT_CLASS],
                    profiles=[bluetooth.SERIAL_PORT_PROFILE]
                )

                print(f"[Attempt {attempt+1}/{max_retries}] Awaiting on CHANNEL {port} (UUID: {uuid})")
                print("Ensure Android is paired and connecting...")
                self.client_sock, client_info = self.server_sock.accept()
                print(f"Connected from: {client_info}")
                self.connection_status = 1
                self.server_sock.settimeout(None)
                return  # Success

            except bluetooth.btcommon.BluetoothError as e:
                print(f"[Attempt {attempt+1}] BluetoothError: {e}")
                self._cleanup_sockets()
                if attempt < max_retries - 1:
                    print(f"Retrying in {retry_delay}s...")
                    time.sleep(retry_delay)
                else:
                    raise
            except Exception as e:
                print(f"Unexpected error: {e}")
                self._cleanup_sockets()
                raise
        
    def _cleanup_sockets(self):
        if hasattr(self, 'client_sock') and self.client_sock:
            try: self.client_sock.close()
            except: pass
        if hasattr(self, 'server_sock') and self.server_sock:
            try: self.server_sock.close()
            except: pass


    def disconnect(self) -> None:
        """Disconnect from Android Bluetooth connection and shutdown all the sockets established"""
        try:
            logger.debug("Disconnecting Bluetooth link")
            self.server_sock.shutdown(socket.SHUT_RDWR)
            self.client_sock.shutdown(socket.SHUT_RDWR)
            self.client_sock.close()
            self.server_sock.close()
            del self.client_sock
            del self.server_sock
            logger.info("Disconnected Bluetooth link")
        except Exception as e:
            logger.error(f"Failed to disconnect Bluetooth link: {e}")

    def send(self, message: AndroidMessage) -> None:
        """Send message to Android"""
        try:
            self.client_sock.send(f"{message.to_string()}\n".encode("utf-8"))
            logger.debug(f"android: {message.jsonify}")
        except OSError as e:
            logger.error(f"android: {e}")
            raise e

    def recv(self) -> Optional[str]:
        """Receive message from Android"""
        try:
            tmp = self.client_sock.recv(1024)
            logger.debug(tmp)
            message = tmp.strip().decode("utf-8")
            logger.debug(f"android: {message}")
            return message
        except OSError as e:
            logger.error(f"android: {e}")
            raise e
        
    def get_connection_status(self) -> bool:
        """Check if client socket is connected."""
        if not hasattr(self, 'client_sock') or self.client_sock is None:
            return False
        try:
            # PyBluez socket inherits socket.getpeername()
            self.client_sock.getpeername()  # raises if disconnected [web:51]
            return True
        except Exception:
            return False
