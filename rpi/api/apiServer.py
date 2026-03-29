from fastapi import FastAPI, Response
from fastapi.middleware.cors import CORSMiddleware
from typing import Dict, Any, List
import re

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["*"],
)

@app.options("/algo-result")
@app.options("/algo-result/")
def options_algo_result() -> Response:
    return Response(status_code=204)

@app.post("/algo-result")
@app.post("/algo-result/")
def process_flexible(payload: Dict[str, Any]) -> Dict:
    global instructions
    
    instructions = payload.get("instructions", [])
    return {"result": f"ACK: {str(instructions)}"}

@app.get("/algo-result")
@app.get("/algo-result/")
def get_algo_result() -> Dict:
    return {"instructions": f"ACK: {instructions}"}

# uvicorn apiTestServer:app --host 0.0.0.0 --port 8000
