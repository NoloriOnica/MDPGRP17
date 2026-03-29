from constant.settings import IMGREG_IP, IMGREG_PORT, ALGO_IP, ALGO_PORT
from fastapi import FastAPI, Response
from fastapi.middleware.cors import CORSMiddleware
from typing import Dict, Any, List
import re
import requests


# Algo
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

def parse_to_algo(s):
    blocks_str = s.split("|", 1)[1]

    pattern = re.compile(r'\{[^}]+\}')
    matches = pattern.findall(blocks_str)

    json_parts = []
    for match in matches:
        inner = match[1:-1]
        id_val = re.search(r'id:\s*(\d+)', inner).group(1)
        x_val  = re.search(r'x:\s*(\d+)',  inner).group(1)
        y_val  = re.search(r'y:\s*(\d+)',  inner).group(1)
        d_val  = re.search(r'd:\s*([NEWS])', inner).group(1)
        json_parts.append(f'{{"id":{id_val},"x":{int(x_val)*10},"y":{int(y_val)*10},"d":"{d_val}"}}')

    json_str = '[' + ','.join(json_parts) + ']'

    objs = eval(json_str)
    for i, obj in enumerate(objs, 1):
        obj['id'] = i
    return objs

android_output = "OBS|{id: 0,x: 7,y: 7,d: N}{id: 1,x: 14,y: 8,d: N}{id: 2,x: 7,y: 12,d: N}{id: 3,x: 15,y: 4,d: N}{id: 4,x: 10,y: 4,d: N}{id: 5,x: 16,y: 11,d: N}{id: 6,x: 4,y: 10,d: N}{id: 7,x: 15,y: 11,d: N}"


result = parse_to_algo(android_output)
print(result)
response = requests.post(f'http://{ALGO_IP}:{ALGO_PORT}/obstacles', json={'data': result})
print(response.json())

