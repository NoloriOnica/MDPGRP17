import logging
from typing import Optional

import serial
from .link import Link
from constant.consts import stm32_prefixes
from constant.settings import BAUD_RATE, SERIAL_PORT


logger = logging.getLogger(__name__)


class STMLink(Link):
    def __init__(self) -> None:
        """
        Constructor for STMLink.
        """
        super().__init__()
        self.serial_link: serial.Serial

        # try to connect to STM32
        try:
            self.connect()
        except Exception as e:
            logger.error(f"Failed to connect to STM32: {e}")
            raise e

    def connect(self) -> None:
        """Connect to STM32 using serial UART connection, given the serial port and the baud rate"""
        self.serial_link = serial.Serial(SERIAL_PORT, BAUD_RATE,timeout=2)
        logger.info("Connected to STM32")

    def disconnect(self) -> None:
        """Disconnect from STM32 by closing the serial link that was opened during connect()"""
        self.serial_link.close()
        del self.serial_link
        logger.info("Disconnected from STM32")

    def send(self, message: str) -> None:
        self.serial_link.write(f"{message}\n".encode("utf-8"))
        logger.debug(f"Send STM: {message}")

    def recv(self) -> Optional[str]:
        message = self.serial_link.readline().strip().decode("utf-8")
        logger.debug(f"Recv STM: {message}")
        return message








    # def send_cmd(
    #     self,
    #     flag: str,
    #     #speed: int,
    #     #turn_radius: float
    #     angle: float,
        
    # ) -> None:

    #     cmd = flag #e.g. T
    #     if flag in stm32_prefixes:
    #         cmd += f"{round(angle, 2)}" + "\n" # T += 
    #     self.serial_link.write(f"{cmd}\n".encode("utf-8"))
    #     logger.debug(f"send_cmd stm: {cmd}")
        
        # cmd = f"{flag}{speed}" # cmd = T40
        # if flag in stm32_prefixes:
        #     cmd += f"|{turn_radius}|{round(angle, 2)}" #T40
        # self.serial_link.write(f"{cmd}\n".encode("utf-8"))
        # logger.debug(f"send_cmd stm: {cmd}")


    # def send_cmd_raw(self, cmd: str) -> None:
    #     parts = cmd.split("|")
    #     self.send_cmd(parts[0][0], int(parts[0][1:]), float(parts[1]), float(parts[2]))
    #     # e.g. "T40|-35|90" = [T40,-35,90]
    #         # send_cmd = T, int(40), float(-35), float(90) 
            
      
    # # TODO: TO CHECK WITH STM DATA THEY WANT      
    # def send_cmd(
    #     self,
    #     # cmd: str
    #     cmd
    # ) -> None:
    #     self.serial_link.write(cmd.encode("utf-8"))
    #     print("sent:", cmd.strip())

    #     # parts = cmd.split("|") # "T40|-35|90" = [T40,-35,90]
    #     # flag = parts[0][0]
    #     # speed = parts[0][1:]
    #     # turn_radius = round(float(parts[1]),2)
    #     # angle = round(float(parts[2]),2)

    #     # self.serial_link.write(f"{cmd}\n".encode("utf-8")) 
    #     # logger.debug(f"send_cmd stm: {cmd}")

    #     # expect ACK first, then DONE
    #     # for _ in range(2):
    #     #     line = ser.readline().decode("utf-8", errors="ignore").strip()
    #     #     if line:
    #     #         print("recv:", line)
    
    # # TODO: TO FIND OUT THE DATA RECEIVED FROM STM
    # def wait_receive(self) -> str:
    #     while self.serial_link.in_waiting <= 0:
    #         pass
    #     try:
    #         message = str(self.serial_link.read_all(), "utf-8")
    #         logger.debug(f"wait recv stm: {message}")
    #     except UnicodeDecodeError:
    #         return ""
    #     return message