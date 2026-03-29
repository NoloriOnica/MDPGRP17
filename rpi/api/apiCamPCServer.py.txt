# pip install fastapi uvicorn ultralytics opencv-python
# To run: python imgModelServer.py

from fastapi import FastAPI, File, UploadFile, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse
import cv2
import numpy as np
from ultralytics import YOLO
from datetime import datetime
import uvicorn

app = FastAPI(title="RPi YOLO Server")

# CORS for RPi
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Config
YOLO_MODEL = "best.pt"
CONFIDENCE = 0.25
IMGSZ = 320

# Load model once
model = YOLO(YOLO_MODEL)


@app.post("/detect")
async def detect_image(file: UploadFile = File(...)):
    """RPi sends image, get YOLO detections back"""
    try:
        # Read image
        contents = await file.read()
        nparr = np.frombuffer(contents, np.uint8)
        frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        if frame is None:
            raise HTTPException(400, "Invalid image")
        
        # Run YOLO
        results = model(frame, imgsz=IMGSZ, conf=CONFIDENCE, verbose=False)
        
        # draw boxes
        for box in results.boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            conf = float(box.conf[0])
            cls = int(box.cls[0])
            label_text = f"{model.names[cls]} {conf:.2f}"

            # boxes
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0,255,0), 2)
            cv2.putText(frame, label_text, (x1, y1-10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,255,0), 2)
        
        # C:\Users\65936\Downloads
        save_path = f"C:\\Users\\65936\\Downloads\\{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        cv2.imwrite(save_path, frame)
        print(f"Saved image to: {save_path}")
        # Extract detections
        detections = []
        for r in results:
            if r.boxes is not None:
                for box in r.boxes:
                    cls_id = int(box.cls)
                    conf = float(box.conf)
                    class_name = model.names[cls_id]
                    detections.append({
                        "id": cls_id,
                        "name": class_name,
                        "confidence": round(conf, 2)
                    })
        
        response = {
            "timestamp": datetime.now().isoformat(),
            "detections": detections,
            "count": len(detections)
        }
        
        print(f"Detected {len(detections)} objects")
        return response
        
    except Exception as e:
        print(f"Error: {e}")
        raise HTTPException(500, str(e))

@app.get("/health")
async def health():
    return {"status": "ok", "model": YOLO_MODEL}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
