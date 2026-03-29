from constant.settings import ALGO_IP, ALGO_PORT
import re
import requests
import time


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

def getSTMCommands(post_body_to_algo_server):
    try:
        start_time = time.perf_counter()

        response = requests.post(
            f'http://{ALGO_IP}:{ALGO_PORT}/pathfinding/',
            json=post_body_to_algo_server,
            timeout=100000
        )

        response.raise_for_status()
        cmds_for_stm = response.json()

        print("From getSTMCommands:", cmds_for_stm)

        end_time = time.perf_counter()
        elapsed_time = end_time - start_time
        print(f"Time taken to get algo response: {elapsed_time:.4f} seconds")

        return cmds_for_stm

    except requests.RequestException as e:
        print(f"Error receiving commands from Algo Server: {e}")
        return None
    

# android_output = "OBS|{id: 0,x: 7,y: 7,d: N}{id: 1,x: 14,y: 8,d: N}{id: 2,x: 7,y: 12,d: N}{id: 3,x: 15,y: 4,d: N}{id: 4,x: 10,y: 4,d: N}{id: 5,x: 16,y: 11,d: N}{id: 6,x: 4,y: 10,d: N}{id: 7,x: 15,y: 11,d: N}"


# result = parse_to_algo(android_output)
# print(result)
# response = requests.post(f'http://{ALGO_IP}:{ALGO_PORT}/obstacles', json={'data': result})
# print(response.json())

