import json
import re


DIRECTION_MAP = {
    "N": "NORTH",
    "S": "SOUTH",
    "E": "EAST",
    "W": "WEST",
}


def parse_obs_string(obs_string: str) -> dict:
    pattern = r"\{id:\s*(\d+),x:\s*(\d+),y:\s*(\d+),d:\s*([NSEW])\}"
    matches = re.findall(pattern, obs_string)

    obstacles = []

    for obs_id, x, y, d in matches:
        x = int(x) * 10
        y = int(y) * 10

        obstacle = {
            "image_id": int(obs_id),
            "direction": DIRECTION_MAP[d],
            "south_west": {
                "x": x,
                "y": y
            },
            "north_east": {
                "x": x + 9,
                "y": y + 9
            }
        }
        obstacles.append(obstacle)

    result = {
        "verbose": False,
        "robot": {
            "direction": "NORTH",
            "south_west": {"x": 1, "y": 1},
            "north_east": {"x": 20, "y": 20}
        },
        "obstacles": obstacles
    }

    return result


