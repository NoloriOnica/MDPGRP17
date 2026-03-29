
export type Point = [number, number, number]; // [x, y, theta]

export type Direction = 'N' | 'S' | 'W' | 'E';

export interface ObstacleSource {
  id?: number | string;
  x: number;
  y: number;
  direction: Direction;
}

export type Rect = [number, number, number, number]; // [xmin, xmax, ymin, ymax]

export interface DubinsEdge {
  type: string;
  segments: [number, number, number];
  length: number;
}

export interface Command {
  type:
    | 'LEFT'
    | 'RIGHT'
    | 'FORWARD'
    | 'BACKLEFT'
    | 'BACKRIGHT'
    | 'BACKWARD'
    | 'PAUSE'
    | 'READ';
  value: number;
}
