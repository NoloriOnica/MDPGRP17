
export const X_MIN = 0;
export const X_MAX = 200; // 200cm = 2 meters
export const Y_MIN = 0;
export const Y_MAX = 200;

export const DEFAULT_LEFT_TURN_RADIUS = 13.5;  // 13.5cm left turning radius
export const DEFAULT_RIGHT_TURN_RADIUS = 17;   // 17cm right turning radius
export const DEFAULT_RADIUS = (DEFAULT_LEFT_TURN_RADIUS + DEFAULT_RIGHT_TURN_RADIUS) / 2; // Compatibility
export const DEFAULT_START_POSE: [number, number, number] = [0, 0, Math.PI / 2];

export const DEFAULT_APPROACH_OFFSET = 30; // 30cm away from obstacle
export const DEFAULT_SAFETY_BUFFER = 2;    // 2cm extra clearance
export const PHYSICAL_OBSTACLE_SIZE = 10;  // 10cm obstacle
export const DEFAULT_ROBOT_WIDTH = 20;     // 20cm width
export const DEFAULT_ROBOT_LENGTH = 20;    // 20cm length
