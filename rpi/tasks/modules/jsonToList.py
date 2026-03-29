def convert_commands(data):
    cmds = []

    mapping = {
        "FORWARD_RIGHT": "RIGHT 90",
        "FORWARD_LEFT": "LEFT 90",
        "BACKWARD_RIGHT": "BACKRIGHT 90",
        "BACKWARD_LEFT": "BACKLEFT 90",
        "CAPTURE_IMAGE": "SNAP"
    }

    for seg in data:
        for ins in seg["instructions"]:
            if isinstance(ins, dict):
                move = ins["move"]
                amount = ins["amount"] * 10
                cmds.append(f"{move} {amount}")
            else:
                cmds.append(mapping.get(ins, ins))

    return cmds

# {"segments":[{"cost":0,"image_id":1,"instructions":[{"amount":70,"move":"FORWARD"},"BACKWARD_RIGHT",{"amount":20,"move":"BACKWARD"},"BACKWARD_RIGHT","CAPTURE_IMAGE"],"path":[]},{"cost":0,"image_id":4,"instructions":["FORWARD_RIGHT","FORWARD_LEFT",{"amount":30,"move":"BACKWARD"},"BACKWARD_LEFT","BACKWARD_LEFT",{"amount":5,"move":"BACKWARD"},"CAPTURE_IMAGE"],"path":[]},{"cost":0,"image_id":5,"instructions":[{"amount":5,"move":"FORWARD"},"FORWARD_LEFT","FORWARD_RIGHT",{"amount":20,"move":"FORWARD"},"FORWARD_RIGHT",{"amount":25,"move":"FORWARD"},"BACKWARD_LEFT",{"amount":5,"move":"FORWARD"},"CAPTURE_IMAGE"],"path":[]},{"cost":0,"image_id":7,"instructions":["FORWARD_LEFT",{"amount":60,"move":"FORWARD"},"CAPTURE_IMAGE"],"path":[]},{"cost":0,"image_id":6,"instructions":[{"amount":10,"move":"BACKWARD"},"FORWARD_RIGHT",{"amount":25,"move":"BACKWARD"},"FORWARD_RIGHT","CAPTURE_IMAGE"],"path":[]},{"cost":0,"image_id":2,"instructions":["BACKWARD_LEFT",{"amount":20,"move":"BACKWARD"},"BACKWARD_LEFT","CAPTURE_IMAGE"],"path":[]},{"cost":0,"image_id":3,"instructions":["FORWARD_LEFT",{"amount":45,"move":"FORWARD"},"BACKWARD_LEFT","CAPTURE_IMAGE"],"path":[]}]}