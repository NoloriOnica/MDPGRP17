from constant.settings import ALGO_IP, ALGO_PORT
import re
import requests


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
        cmds_for_stm = requests.post(f'http://{ALGO_IP}:{ALGO_PORT}/obstacles', json={'data': post_body_to_algo_server}, timeout=600).json()
    except requests.RequestException as e:
        print(f"Error receiving commands from Algo Server: {e}")
        
    return cmds_for_stm

