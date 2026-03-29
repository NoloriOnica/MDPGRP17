
import { Point, Rect, DubinsEdge, Command } from '../types';
import { X_MIN, X_MAX, Y_MIN, Y_MAX } from '../constants';

export const mod2pi = (theta: number): number => {
  return theta - 2.0 * Math.PI * Math.floor(theta / (2.0 * Math.PI));
};

export const euclidean = (p: Point, q: Point): number => {
  return Math.hypot(p[0] - q[0], p[1] - q[1]);
};

export const withinBounds = (x: number, y: number): boolean => {
  return x >= X_MIN && x <= X_MAX && y >= Y_MIN && y <= Y_MAX;
};

export const sampleDubinsPath = (q0: Point, pathType: string, segments: [number, number, number], R: number, step = 1.0): Point[] => {
  let [x, y, theta] = q0;
  const samples: Point[] = [[x, y, theta]];
  const motions = pathType.split('') as ('L' | 'S' | 'R')[];

  for (let i = 0; i < motions.length; i++) {
    const motion = motions[i];
    const seg = segments[i];
    const length = seg * R;
    let traveled = 0.0;

    while (traveled < length) {
      const ds = Math.min(step, length - traveled);

      if (motion === 'S') {
        x += ds * Math.cos(theta);
        y += ds * Math.sin(theta);
      } else if (motion === 'L') {
        const dtheta = ds / R;
        const theta_start = theta;
        theta += dtheta;
        x += R * (Math.sin(theta) - Math.sin(theta_start));
        y -= R * (Math.cos(theta) - Math.cos(theta_start));
      } else if (motion === 'R') {
        const dtheta = ds / R;
        const theta_start = theta;
        theta -= dtheta;
        x += R * (Math.sin(theta_start) - Math.sin(theta));
        y += R * (Math.cos(theta) - Math.cos(theta_start));
      }
      traveled += ds;
      samples.push([x, y, theta]);
    }
  }
  return samples;
};

const pointInRect = (x: number, y: number, rect: Rect): boolean => {
  const [xmin, xmax, ymin, ymax] = rect;
  return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
};

const pathCollides = (points: Point[], obstacles: Rect[]): boolean => {
  for (const [px, py] of points) {
    for (const obs of obstacles) {
      if (pointInRect(px, py, obs)) return true;
    }
  }
  return false;
};

const pathOutOfBounds = (points: Point[]): boolean => {
  for (const [px, py] of points) {
    if (!withinBounds(px, py)) return true;
  }
  return false;
};

export const findValidDubinsPath = (q0: Point, q1: Point, R: number, obstacles: Rect[]): DubinsEdge | null => {
  const dx = q1[0] - q0[0];
  const dy = q1[1] - q0[1];
  const D = Math.hypot(dx, dy);
  const d = D / R;
  const angle = Math.atan2(dy, dx);
  const alpha = mod2pi(q0[2] - angle);
  const beta = mod2pi(q1[2] - angle);

  const candidates: Record<string, [number, number, number] | null> = {
    LSL: LSL(alpha, beta, d),
    RSR: RSR(alpha, beta, d),
    LSR: LSR(alpha, beta, d),
    RSL: RSL(alpha, beta, d),
    RLR: RLR(alpha, beta, d),
    LRL: LRL(alpha, beta, d),
  };

  let best: DubinsEdge | null = null;
  let minLength = Infinity;

  for (const [type, segs] of Object.entries(candidates)) {
    if (!segs) continue;
    const samples = sampleDubinsPath(q0, type, segs, R, 1.0);
    if (pathCollides(samples, obstacles)) continue;
    if (pathOutOfBounds(samples)) continue;

    const length = (segs[0] + segs[1] + segs[2]) * R;
    if (length < minLength) {
      minLength = length;
      best = { type, segments: segs, length };
    }
  }

  return best;
};

const LSL = (alpha: number, beta: number, d: number): [number, number, number] | null => {
  const tmp = d + Math.sin(alpha) - Math.sin(beta);
  const p_sq = 2 + d * d - 2 * Math.cos(alpha - beta) + 2 * d * (Math.sin(alpha) - Math.sin(beta));
  if (p_sq < 0) return null;
  const p = Math.sqrt(p_sq);
  const t = mod2pi(-alpha + Math.atan2(Math.cos(beta) - Math.cos(alpha), tmp));
  const q = mod2pi(beta - Math.atan2(Math.cos(beta) - Math.cos(alpha), tmp));
  return [t, p, q];
};

const RSR = (alpha: number, beta: number, d: number): [number, number, number] | null => {
  const tmp = d - Math.sin(alpha) + Math.sin(beta);
  const p_sq = 2 + d * d - 2 * Math.cos(alpha - beta) + 2 * d * (Math.sin(beta) - Math.sin(alpha));
  if (p_sq < 0) return null;
  const p = Math.sqrt(p_sq);
  const t = mod2pi(alpha - Math.atan2(Math.cos(alpha) - Math.cos(beta), tmp));
  const q = mod2pi(-beta + Math.atan2(Math.cos(alpha) - Math.cos(beta), tmp));
  return [t, p, q];
};

const LSR = (alpha: number, beta: number, d: number): [number, number, number] | null => {
  const sa = Math.sin(alpha), sb = Math.sin(beta);
  const ca = Math.cos(alpha), cb = Math.cos(beta);
  const c_ab = Math.cos(alpha - beta);
  const p_sq = -2 + (d * d) + (2 * c_ab) + (2 * d * (sa + sb));
  if (p_sq < 0) return null;
  const p = Math.sqrt(p_sq);
  const tmp2 = Math.atan2((-ca - cb), (d + sa + sb)) - Math.atan2(-2.0, p);
  const t = mod2pi(-alpha + tmp2);
  const q = mod2pi(-mod2pi(beta) + tmp2);
  return [t, p, q];
};

const RSL = (alpha: number, beta: number, d: number): [number, number, number] | null => {
  const sa = Math.sin(alpha), sb = Math.sin(beta);
  const ca = Math.cos(alpha), cb = Math.cos(beta);
  const c_ab = Math.cos(alpha - beta);
  const p_sq = (d * d) - 2 + (2 * c_ab) - (2 * d * (sa + sb));
  if (p_sq < 0) return null;
  const p = Math.sqrt(p_sq);
  const tmp2 = Math.atan2((ca + cb), (d - sa - sb)) - Math.atan2(2.0, p);
  const t = mod2pi(alpha - tmp2);
  const q = mod2pi(mod2pi(beta) - tmp2);
  return [t, p, q];
};

const RLR = (alpha: number, beta: number, d: number): [number, number, number] | null => {
  const tmp = (6.0 - d * d + 2.0 * Math.cos(alpha - beta) + 2.0 * d * (Math.sin(alpha) - Math.sin(beta))) / 8.0;
  if (Math.abs(tmp) > 1) return null;
  const p = mod2pi(Math.acos(tmp));
  const t = mod2pi(alpha - Math.atan2(Math.cos(alpha) - Math.cos(beta), d - Math.sin(alpha) + Math.sin(beta)) + p / 2.0);
  const q = mod2pi(alpha - beta - t + p);
  return [t, p, q];
};

const LRL = (alpha: number, beta: number, d: number): [number, number, number] | null => {
  const tmp = (6.0 - d * d + 2.0 * Math.cos(alpha - beta) + 2.0 * d * (Math.sin(beta) - Math.sin(alpha))) / 8.0;
  if (Math.abs(tmp) > 1) return null;
  const p = mod2pi(Math.acos(tmp));
  const t = mod2pi(-alpha - Math.atan2(Math.cos(alpha) - Math.cos(beta), d + Math.sin(alpha) - Math.sin(beta)) + p / 2.0);
  const q = mod2pi(beta - alpha - t + p);
  return [t, p, q];
};

export const segmentsToCommands = (pathType: string, segments: [number, number, number], R: number): Command[] => {
  const commands: Command[] = [];
  const motions = pathType.split('');
  for (let i = 0; i < motions.length; i++) {
    const motion = motions[i];
    const seg = segments[i];
    const dist = seg * R;
    if (seg <= 0.001) continue;

    if (motion === 'L') {
      const degrees = (dist / R) * (180 / Math.PI);
      commands.push({ type: 'LEFT', value: Math.max(1, Math.round(degrees)) });
    } else if (motion === 'R') {
      const degrees = (dist / R) * (180 / Math.PI);
      commands.push({ type: 'RIGHT', value: Math.max(1, Math.round(degrees)) });
    } else if (motion === 'S') {
      commands.push({ type: 'FORWARD', value: parseFloat(dist.toFixed(1)) });
    }
  }
  return commands;
};
