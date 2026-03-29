from picamera2 import Picamera2
import cv2
import numpy as np

def main():
    cv2.startWindowThread()
    picam2 = Picamera2()
    picam2.configure(picam2.create_preview_configuration(main={"format": 'XRGB8888', "size": (640, 480)}))
    picam2.start()
    while True:
        # Capture frame-by-frame
        im = picam2.capture_array()
    
        # Display the resulting frame
        cv2.imshow("Camera", im)
    
        # Break the loop if 'q' is pressed
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    # Release the camera and close windows
    cv2.destroyAllWindows()
if __name__ == "__main__":
    main()