
import React, { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import { 
  Play, Pause, RotateCcw, Settings2, List, Navigation, Trash2, 
  PlusCircle, FileText, X, Eye, 
  Tablet, CheckCircle2, ShieldAlert, Target, Grid3X3,
  Cpu, Move, ArrowUp, ArrowDown, ArrowLeft, ArrowRight, Download
} from 'lucide-react';
import { ObstacleSource, Point, Rect, Direction, Command } from './types';
import { 
  X_MIN, X_MAX, Y_MIN, Y_MAX, DEFAULT_START_POSE,
  DEFAULT_LEFT_TURN_RADIUS, DEFAULT_RIGHT_TURN_RADIUS,
  DEFAULT_APPROACH_OFFSET, DEFAULT_SAFETY_BUFFER, PHYSICAL_OBSTACLE_SIZE,
  DEFAULT_ROBOT_WIDTH, DEFAULT_ROBOT_LENGTH
} from './constants';
import { findValidDubinsPath, sampleDubinsPath, segmentsToCommands } from './services/dubinsService';
import { solveTSP } from './services/tspService';

type PlannedSample = { pose: Point; isAtGoal: boolean };

interface PathPlanResult {
  pathOrder: number[];
  samples: PlannedSample[];
  commands: Command[];
  unreachableIndices: number[];
}

interface SimulatorDefaults {
  plannerMode: PlannerMode;
  robotWidth: number;
  robotLength: number;
  rearTrack: number;
  leftTurnRadius: number;
  rightTurnRadius: number;
  approachOffset: number;
  safetyBuffer: number;
}

const DEFAULT_SETTINGS_STORAGE_KEY = 'robot-simulator-default-settings-v2';

const FALLBACK_DEFAULT_SETTINGS: SimulatorDefaults = {
  plannerMode: 'legacy',
  robotWidth: DEFAULT_ROBOT_WIDTH,
  robotLength: DEFAULT_ROBOT_LENGTH,
  rearTrack: 19,
  leftTurnRadius: DEFAULT_LEFT_TURN_RADIUS,
  rightTurnRadius: DEFAULT_RIGHT_TURN_RADIUS,
  approachOffset: DEFAULT_APPROACH_OFFSET,
  safetyBuffer: DEFAULT_SAFETY_BUFFER
};

type PlannerMode = 'legacy' | 'realistic';

const clampNumber = (value: number, min: number, max: number): number => {
  return Math.min(max, Math.max(min, value));
};

const getEffectiveTurnRadius = (leftTurnRadius: number, rightTurnRadius: number): number => {
  return (leftTurnRadius + rightTurnRadius) / 2;
};

const LOOK_BACK_LIMIT_CM = 60;
const LOOK_BACK_SAMPLE_STEP_CM = 5;
const LOOK_BACK_EPS = 1e-6;
const ORDER_PREFERENCE_WEIGHT = 2.0;
const COMPLETENESS_CONNECTIVITY_BONUS = 6.0;

const sanitizeDefaultSettings = (candidate: Partial<SimulatorDefaults>): SimulatorDefaults => {
  const source = { ...FALLBACK_DEFAULT_SETTINGS, ...candidate };
  return {
    plannerMode: source.plannerMode === 'realistic' ? 'realistic' : 'legacy',
    robotWidth: clampNumber(Number(source.robotWidth), 10, 60),
    robotLength: clampNumber(Number(source.robotLength), 10, 60),
    rearTrack: clampNumber(Number(source.rearTrack), 10, 40),
    leftTurnRadius: clampNumber(Number(source.leftTurnRadius), 4, 40),
    rightTurnRadius: clampNumber(Number(source.rightTurnRadius), 4, 40),
    approachOffset: clampNumber(Number(source.approachOffset), 10, 180),
    safetyBuffer: clampNumber(Number(source.safetyBuffer), 0, 20)
  };
};

const loadSavedDefaults = (): SimulatorDefaults => {
  if (typeof window === 'undefined') return FALLBACK_DEFAULT_SETTINGS;
  try {
    const raw = window.localStorage.getItem(DEFAULT_SETTINGS_STORAGE_KEY);
    if (!raw) return FALLBACK_DEFAULT_SETTINGS;
    const parsed = JSON.parse(raw) as Partial<SimulatorDefaults>;
    return sanitizeDefaultSettings(parsed);
  } catch {
    return FALLBACK_DEFAULT_SETTINGS;
  }
};

const saveDefaults = (settings: SimulatorDefaults): void => {
  if (typeof window === 'undefined') return;
  window.localStorage.setItem(DEFAULT_SETTINGS_STORAGE_KEY, JSON.stringify(settings));
};

const isObject = (value: unknown): value is Record<string, unknown> => {
  return typeof value === 'object' && value !== null;
};

const normalizeDirection = (value: unknown): Direction => {
  const token = typeof value === 'string' ? value.toUpperCase() : '';
  if (token === 'N' || token === 'S' || token === 'E' || token === 'W') {
    return token;
  }
  return 'E';
};

const parseJsonLike = (value: string): unknown => {
  try {
    return JSON.parse(value);
  } catch {
    // Handle Python-style stringified dict/list returned by some backend prototypes.
    return JSON.parse(value.replace(/'/g, '"'));
  }
};

const buildGoalsFromObstacles = (
  source: ObstacleSource[],
  approachOffset: number,
  vehicleLength: number,
  vehicleWidth: number
): Point[] => {
  const g: Point[] = [DEFAULT_START_POSE];
  const frontToBackWheel = Math.max(vehicleLength - 2, LOOK_BACK_EPS);
  const lateralOffset = Math.max(vehicleWidth, 0) / 2;
  const footprintReadingBonus = Math.max(0, (Math.max(vehicleWidth, 0) - PHYSICAL_OBSTACLE_SIZE) / 2);
  const effectiveReadingDistance = approachOffset + footprintReadingBonus;
  source.forEach(o => {
    let theta = 0;
    let pictureX = o.x + PHYSICAL_OBSTACLE_SIZE / 2;
    let pictureY = o.y + PHYSICAL_OBSTACLE_SIZE / 2;
    let outwardX = 0;
    let outwardY = 0;

    switch (o.direction) {
      case 'N':
        pictureY = o.y + PHYSICAL_OBSTACLE_SIZE;
        outwardY = 1;
        theta = -Math.PI / 2;
        break;
      case 'S':
        pictureY = o.y;
        outwardY = -1;
        theta = Math.PI / 2;
        break;
      case 'W':
        pictureX = o.x;
        outwardX = -1;
        theta = 0.0;
        break;
      case 'E':
        pictureX = o.x + PHYSICAL_OBSTACLE_SIZE;
        outwardX = 1;
        theta = Math.PI;
        break;
    }

    const frontX = pictureX + outwardX * effectiveReadingDistance;
    const frontY = pictureY + outwardY * effectiveReadingDistance;
    const headingX = Math.cos(theta);
    const headingY = Math.sin(theta);
    const leftX = -Math.sin(theta);
    const leftY = Math.cos(theta);

    const gx = frontX - headingX * frontToBackWheel - leftX * lateralOffset;
    const gy = frontY - headingY * frontToBackWheel - leftY * lateralOffset;
    g.push([gx, gy, theta]);
  });
  return g;
};

const buildGoalsFromObstaclesRealistic = (
  source: ObstacleSource[],
  approachOffset: number,
  vehicleLength: number,
  vehicleWidth: number,
  rearTrack: number
): Point[] => {
  const startTheta = DEFAULT_START_POSE[2];
  const startRightX = Math.sin(startTheta);
  const startRightY = -Math.cos(startTheta);
  const startCenter: Point = [
    DEFAULT_START_POSE[0] + (vehicleLength / 2) * Math.cos(startTheta) + (vehicleWidth / 2) * startRightX,
    DEFAULT_START_POSE[1] + (vehicleLength / 2) * Math.sin(startTheta) + (vehicleWidth / 2) * startRightY,
    startTheta
  ];
  const goals: Point[] = [startCenter];
  const effectiveReadingDistance = Math.max(0, approachOffset);

  source.forEach(o => {
    let theta = 0;
    let pictureX = o.x + PHYSICAL_OBSTACLE_SIZE / 2;
    let pictureY = o.y + PHYSICAL_OBSTACLE_SIZE / 2;
    let outwardX = 0;
    let outwardY = 0;

    switch (o.direction) {
      case 'N':
        pictureY = o.y + PHYSICAL_OBSTACLE_SIZE;
        outwardY = 1;
        theta = -Math.PI / 2;
        break;
      case 'S':
        pictureY = o.y;
        outwardY = -1;
        theta = Math.PI / 2;
        break;
      case 'W':
        pictureX = o.x;
        outwardX = -1;
        theta = 0;
        break;
      case 'E':
        pictureX = o.x + PHYSICAL_OBSTACLE_SIZE;
        outwardX = 1;
        theta = Math.PI;
        break;
    }

    const cameraX = pictureX + outwardX * effectiveReadingDistance;
    const cameraY = pictureY + outwardY * effectiveReadingDistance;
    const gx = cameraX - (vehicleLength / 2) * Math.cos(theta);
    const gy = cameraY - (vehicleLength / 2) * Math.sin(theta);
    goals.push([gx, gy, theta]);
  });

  return goals;
};

const buildRectObstacles = (source: ObstacleSource[], safetyBuffer: number): Rect[] => {
  return source.map(o => [
    o.x - safetyBuffer,
    o.x + PHYSICAL_OBSTACLE_SIZE + safetyBuffer,
    o.y - safetyBuffer,
    o.y + PHYSICAL_OBSTACLE_SIZE + safetyBuffer
  ]);
};

const computePathPlan = (
  source: ObstacleSource[],
  leftTurnRadius: number,
  rightTurnRadius: number,
  approachOffset: number,
  vehicleLength: number,
  vehicleWidth: number,
  safetyBuffer: number
): PathPlanResult => {
  const effectiveTurnRadius = getEffectiveTurnRadius(leftTurnRadius, rightTurnRadius);
  const goals = buildGoalsFromObstacles(source, approachOffset, vehicleLength, vehicleWidth);
  const rectObstacles = buildRectObstacles(source, safetyBuffer);
  const preferredOrder = solveTSP(goals, effectiveTurnRadius, rectObstacles);
  const preferredRank = new Map<number, number>();
  preferredOrder.forEach((node, rank) => {
    if (node === 0 || preferredRank.has(node)) return;
    preferredRank.set(node, rank);
  });

  const samples: PlannedSample[] = [];
  const cmds: Command[] = [];
  const scannedTargets = new Set<number>();
  const remainingTargets = new Set<number>();
  for (let i = 1; i < goals.length; i++) remainingTargets.add(i);

  let currentIdx = 0;
  const actualOrder: number[] = [0];
  const edgeHistory: Array<{
    from: number;
    to: number;
    samples: Point[];
    commands: Command[];
    isFullEdge: boolean;
  }> = [];

  const commandTravelDistance = (cmd: Command): number => {
    if (cmd.type === 'FORWARD' || cmd.type === 'BACKWARD') return Math.abs(cmd.value);
    if (cmd.type === 'LEFT' || cmd.type === 'RIGHT' || cmd.type === 'BACKLEFT' || cmd.type === 'BACKRIGHT') {
      return Math.abs((cmd.value * Math.PI / 180) * effectiveTurnRadius);
    }
    return 0;
  };

  const invertMovementByDistance = (cmd: Command, distance: number): Command | null => {
    if (distance <= LOOK_BACK_EPS) return null;
    if (cmd.type === 'FORWARD') return { type: 'BACKWARD', value: Number(distance.toFixed(1)) };
    if (cmd.type === 'BACKWARD') return { type: 'FORWARD', value: Number(distance.toFixed(1)) };
    if (cmd.type === 'LEFT') {
      const degrees = Math.max(1, Math.round((distance / effectiveTurnRadius) * (180 / Math.PI)));
      return { type: 'BACKLEFT', value: degrees };
    }
    if (cmd.type === 'RIGHT') {
      const degrees = Math.max(1, Math.round((distance / effectiveTurnRadius) * (180 / Math.PI)));
      return { type: 'BACKRIGHT', value: degrees };
    }
    if (cmd.type === 'BACKLEFT') {
      const degrees = Math.max(1, Math.round((distance / effectiveTurnRadius) * (180 / Math.PI)));
      return { type: 'LEFT', value: degrees };
    }
    if (cmd.type === 'BACKRIGHT') {
      const degrees = Math.max(1, Math.round((distance / effectiveTurnRadius) * (180 / Math.PI)));
      return { type: 'RIGHT', value: degrees };
    }
    return null;
  };

  const buildPartialReverseCommands = (segmentCommands: Command[], reverseDistance: number): Command[] => {
    let remaining = Math.max(0, reverseDistance);
    const reverseCommands: Command[] = [];

    for (let i = segmentCommands.length - 1; i >= 0 && remaining > LOOK_BACK_EPS; i--) {
      const cmd = segmentCommands[i];
      const cmdLen = commandTravelDistance(cmd);
      if (cmdLen <= LOOK_BACK_EPS) continue;

      const useLen = Math.min(cmdLen, remaining);
      const inverse = invertMovementByDistance(cmd, useLen);
      if (inverse) reverseCommands.push(inverse);
      remaining -= useLen;
    }

    return reverseCommands;
  };

  const sampleLookBackCandidates = (
    segmentSamples: Point[]
  ): Array<{ sampleIndex: number; reverseDistance: number }> => {
    if (segmentSamples.length === 0) return [];
    const candidates: Array<{ sampleIndex: number; reverseDistance: number }> = [
      { sampleIndex: segmentSamples.length - 1, reverseDistance: 0 }
    ];
    if (segmentSamples.length < 2 || LOOK_BACK_LIMIT_CM <= LOOK_BACK_EPS) {
      return candidates;
    }

    let traveled = 0;
    let lastMarkedDistance = 0;
    let farthestIndex = segmentSamples.length - 1;
    let farthestDistance = 0;

    for (let idx = segmentSamples.length - 2; idx >= 0; idx--) {
      const [x1, y1] = segmentSamples[idx + 1];
      const [x0, y0] = segmentSamples[idx];
      traveled += Math.hypot(x1 - x0, y1 - y0);
      if (traveled > LOOK_BACK_LIMIT_CM + LOOK_BACK_EPS) break;

      farthestIndex = idx;
      farthestDistance = traveled;
      if (traveled - lastMarkedDistance >= LOOK_BACK_SAMPLE_STEP_CM - LOOK_BACK_EPS) {
        candidates.push({ sampleIndex: idx, reverseDistance: traveled });
        lastMarkedDistance = traveled;
      }
    }

    if (
      farthestDistance > LOOK_BACK_EPS &&
      candidates[candidates.length - 1]?.sampleIndex !== farthestIndex
    ) {
      candidates.push({ sampleIndex: farthestIndex, reverseDistance: farthestDistance });
    }

    return candidates;
  };

  const evaluateTransition = (
    targetIdx: number,
    lastEdge: (typeof edgeHistory)[number] | undefined
  ): {
    targetIdx: number;
    startPose: Point;
    edge: DubinsEdge;
    reverseDistance: number;
    reverseSampleIndex: number;
    transitionCost: number;
  } | null => {
    const q0 = goals[currentIdx];
    const q1 = goals[targetIdx];
    let startPose = q0;
    let edge = findValidDubinsPath(startPose, q1, effectiveTurnRadius, rectObstacles);
    let bestTotalCost = edge ? edge.length : Number.POSITIVE_INFINITY;
    let reverseDistance = 0;
    let reverseSampleIndex = -1;

    if (lastEdge && lastEdge.to === currentIdx && lastEdge.samples.length > 1) {
      const lookBackCandidates = sampleLookBackCandidates(lastEdge.samples);
      for (const candidate of lookBackCandidates.slice(1)) {
        const candidatePose = lastEdge.samples[candidate.sampleIndex];
        const candidateEdge = findValidDubinsPath(
          candidatePose,
          q1,
          effectiveTurnRadius,
          rectObstacles
        );
        if (!candidateEdge) continue;

        const totalCost = candidate.reverseDistance + candidateEdge.length;
        if (totalCost + LOOK_BACK_EPS < bestTotalCost) {
          bestTotalCost = totalCost;
          startPose = candidatePose;
          edge = candidateEdge;
          reverseDistance = candidate.reverseDistance;
          reverseSampleIndex = candidate.sampleIndex;
        }
      }
    }

    if (!edge) return null;

    return {
      targetIdx,
      startPose,
      edge,
      reverseDistance,
      reverseSampleIndex,
      transitionCost: bestTotalCost
    };
  };

  type TransitionChoice = NonNullable<ReturnType<typeof evaluateTransition>> & { score: number };

  while (remainingTargets.size > 0) {
    const lastEdge = edgeHistory[edgeHistory.length - 1];
    let bestChoice: TransitionChoice | null = null;

    for (const targetIdx of remainingTargets) {
      const transition = evaluateTransition(targetIdx, lastEdge);
      if (!transition) continue;

      const rank = preferredRank.get(targetIdx) ?? goals.length;
      let connectivity = 0;
      for (const otherIdx of remainingTargets) {
        if (otherIdx === targetIdx) continue;
        if (findValidDubinsPath(goals[targetIdx], goals[otherIdx], effectiveTurnRadius, rectObstacles)) {
          connectivity += 1;
        }
      }

      const score =
        transition.transitionCost +
        rank * ORDER_PREFERENCE_WEIGHT -
        connectivity * COMPLETENESS_CONNECTIVITY_BONUS;

      if (
        !bestChoice ||
        score < bestChoice.score - LOOK_BACK_EPS ||
        (Math.abs(score - bestChoice.score) <= LOOK_BACK_EPS &&
          transition.transitionCost < bestChoice.transitionCost)
      ) {
        bestChoice = { ...transition, score };
      }
    }

    if (!bestChoice) {
      break;
    }

    if (bestChoice.reverseDistance > LOOK_BACK_EPS && lastEdge && bestChoice.reverseSampleIndex >= 0) {
      const partialReverse = buildPartialReverseCommands(lastEdge.commands, bestChoice.reverseDistance);
      const reverseSamples = [...lastEdge.samples.slice(bestChoice.reverseSampleIndex)].reverse();
      reverseSamples.forEach(pose => samples.push({ pose, isAtGoal: false }));
      cmds.push(...partialReverse);
    }

    const pathSamples = sampleDubinsPath(
      bestChoice.startPose,
      bestChoice.edge.type,
      bestChoice.edge.segments,
      effectiveTurnRadius,
      1.0
    );
    const movementCommands = segmentsToCommands(
      bestChoice.edge.type,
      bestChoice.edge.segments,
      effectiveTurnRadius
    );
    const shouldScanTarget = !scannedTargets.has(bestChoice.targetIdx);

    pathSamples.forEach((pose, idx) => {
      samples.push({
        pose,
        isAtGoal: idx === pathSamples.length - 1 && shouldScanTarget
      });
    });

    cmds.push(...movementCommands);
    if (shouldScanTarget) {
      cmds.push({ type: 'PAUSE', value: 500 });
      cmds.push({ type: 'READ', value: 500 });
      scannedTargets.add(bestChoice.targetIdx);
    }

    edgeHistory.push({
      from: currentIdx,
      to: bestChoice.targetIdx,
      samples: pathSamples,
      commands: movementCommands,
      isFullEdge: bestChoice.reverseDistance <= LOOK_BACK_EPS
    });

    currentIdx = bestChoice.targetIdx;
    actualOrder.push(currentIdx);
    remainingTargets.delete(currentIdx);
  }

  const failed = [...remainingTargets]
    .map(targetIdx => targetIdx - 1)
    .sort((a, b) => a - b);

  return {
    pathOrder: actualOrder,
    samples,
    commands: cmds,
    unreachableIndices: failed
  };
};

const parseObstaclePayload = (payload: unknown): ObstacleSource[] => {
  let raw: unknown = payload;

  if (typeof raw === 'string') {
    raw = parseJsonLike(raw);
  }

  if (isObject(raw)) {
    const container = raw as Record<string, unknown>;
    if (container.data !== undefined) raw = container.data;
    else if (container.obstacles !== undefined) raw = container.obstacles;
    else if (container.block_positions !== undefined) raw = container.block_positions;
  }

  if (typeof raw === 'string') {
    raw = parseJsonLike(raw);
  }

  if (!Array.isArray(raw)) {
    throw new Error('API payload must be an array or an object containing data/obstacles.');
  }

  return raw.map((item, i) => {
    if (!isObject(item)) {
      throw new Error(`Invalid obstacle at index ${i}: expected object.`);
    }

    const rawX = Number(item.x);
    const rawY = Number(item.y);
    if (!Number.isFinite(rawX) || !Number.isFinite(rawY)) {
      throw new Error(`Invalid obstacle at index ${i}: x and y must be numbers.`);
    }

    const scaleX = rawX > 200 ? 0.1 : 1;
    const scaleY = rawY > 200 ? 0.1 : 1;
    const id = typeof item.id === 'number' || typeof item.id === 'string' ? item.id : i + 1;
    const direction = normalizeDirection(item.d ?? item.direction);

    return {
      id,
      x: Math.round(rawX * scaleX),
      y: Math.round(rawY * scaleY),
      direction
    };
  });
};

const toInstructionStrings = (source: Command[]): string[] => {
  return source.map(cmd => {
    const isLinear = cmd.type === 'FORWARD' || cmd.type === 'BACKWARD';
    const value = isLinear ? Number((cmd.value * 10).toFixed(1)) : cmd.value;
    const valueText = Number.isInteger(value) ? String(Math.trunc(value)) : String(value);
    return `${cmd.type} ${valueText}`;
  });
};

const instructionStringsToCommands = (source: string[]): Command[] => {
  const parsed: Command[] = [];
  source.forEach(line => {
    const [rawType, rawValue] = line.trim().split(/\s+/, 2);
    if (!rawType) return;
    const typeToken = rawType.toUpperCase();
    const numeric = Number(rawValue ?? '0');
    const value = Number.isFinite(numeric) ? numeric : 0;

    if (typeToken === 'FORWARD' || typeToken === 'BACKWARD' || typeToken === 'REVERSE') {
      parsed.push({
        type: typeToken === 'REVERSE' ? 'BACKWARD' : typeToken,
        value: Number((value / 10).toFixed(1))
      });
      return;
    }

    if (
      typeToken === 'LEFT' ||
      typeToken === 'RIGHT' ||
      typeToken === 'BACKLEFT' ||
      typeToken === 'BACKRIGHT' ||
      typeToken === 'PAUSE' ||
      typeToken === 'READ'
    ) {
      parsed.push({ type: typeToken as Command['type'], value: Math.round(value) });
    }
  });
  return parsed;
};

const simulateRealisticMotion = (
  pose: Point,
  command: Command,
  vehicleLength: number,
  leftTurnRadius: number,
  rightTurnRadius: number,
  rearTrack: number
): Point[] => {
  const [startX, startY, startTheta] = pose;
  let x = startX;
  let y = startY;
  let theta = startTheta;
  const samples: Point[] = [];

  const linearStep = 1;
  const angularStep = (4 * Math.PI) / 180;

  if (command.type === 'FORWARD' || command.type === 'BACKWARD') {
    const total = Math.max(0, command.value);
    const direction = command.type === 'FORWARD' ? 1 : -1;
    let progressed = 0;
    while (progressed < total - LOOK_BACK_EPS) {
      const ds = Math.min(linearStep, total - progressed);
      x += direction * ds * Math.cos(theta);
      y += direction * ds * Math.sin(theta);
      progressed += ds;
      samples.push([x, y, theta]);
    }
    return samples;
  }

  if (
    command.type !== 'LEFT' &&
    command.type !== 'RIGHT' &&
    command.type !== 'BACKLEFT' &&
    command.type !== 'BACKRIGHT'
  ) {
    return samples;
  }

  const totalAngle = Math.max(0, command.value) * Math.PI / 180;
  if (totalAngle <= LOOK_BACK_EPS) return samples;
  const halfLength = vehicleLength / 2;
  const rightOffsetLeft = (rearTrack / 2) + leftTurnRadius;
  const rightOffsetRight = (rearTrack / 2) + rightTurnRadius;

  if (command.type === 'LEFT' || command.type === 'BACKLEFT') {
    const sideRadius = Math.max(rightOffsetLeft, LOOK_BACK_EPS);
    const cx = x - halfLength * Math.cos(theta) - sideRadius * Math.sin(theta);
    const cy = y - halfLength * Math.sin(theta) + sideRadius * Math.cos(theta);
    const sign = command.type === 'LEFT' ? 1 : -1;
    let progressed = 0;
    while (progressed < totalAngle - LOOK_BACK_EPS) {
      const dtheta = Math.min(angularStep, totalAngle - progressed);
      theta += sign * dtheta;
      x = cx + halfLength * Math.cos(theta) + sideRadius * Math.sin(theta);
      y = cy + halfLength * Math.sin(theta) - sideRadius * Math.cos(theta);
      progressed += dtheta;
      samples.push([x, y, theta]);
    }
    return samples;
  }

  const sideRadius = Math.max(rightOffsetRight, LOOK_BACK_EPS);
  const cx = x - halfLength * Math.cos(theta) + sideRadius * Math.sin(theta);
  const cy = y - halfLength * Math.sin(theta) - sideRadius * Math.cos(theta);
  const sign = command.type === 'RIGHT' ? -1 : 1;
  let progressed = 0;
  while (progressed < totalAngle - LOOK_BACK_EPS) {
    const dtheta = Math.min(angularStep, totalAngle - progressed);
    theta += sign * dtheta;
    x = cx + halfLength * Math.cos(theta) - sideRadius * Math.sin(theta);
    y = cy + halfLength * Math.sin(theta) + sideRadius * Math.cos(theta);
    progressed += dtheta;
    samples.push([x, y, theta]);
  }
  return samples;
};

const buildSamplesFromRealisticCommands = (
  commands: Command[],
  vehicleLength: number,
  vehicleWidth: number,
  leftTurnRadius: number,
  rightTurnRadius: number,
  rearTrack: number
): PlannedSample[] => {
  const startTheta = DEFAULT_START_POSE[2];
  const startRightX = Math.sin(startTheta);
  const startRightY = -Math.cos(startTheta);
  const startPose: Point = [
    DEFAULT_START_POSE[0] + (vehicleLength / 2) * Math.cos(startTheta) + (vehicleWidth / 2) * startRightX,
    DEFAULT_START_POSE[1] + (vehicleLength / 2) * Math.sin(startTheta) + (vehicleWidth / 2) * startRightY,
    startTheta
  ];
  const samples: PlannedSample[] = [{ pose: startPose, isAtGoal: false }];
  let current: Point = startPose;

  commands.forEach(cmd => {
    if (cmd.type === 'PAUSE') return;
    if (cmd.type === 'READ') {
      samples.push({ pose: current, isAtGoal: true });
      return;
    }
    const motionSamples = simulateRealisticMotion(
      current,
      cmd,
      vehicleLength,
      leftTurnRadius,
      rightTurnRadius,
      rearTrack
    );
    motionSamples.forEach(pose => {
      samples.push({ pose, isAtGoal: false });
    });
    if (motionSamples.length > 0) {
      current = motionSamples[motionSamples.length - 1];
    }
  });

  return samples;
};

const App: React.FC = () => {
  const [defaultSettings, setDefaultSettings] = useState<SimulatorDefaults>(() => loadSavedDefaults());
  const [plannerMode, setPlannerMode] = useState<PlannerMode>(defaultSettings.plannerMode);

  // Robot Configuration
  const [robotWidth, setRobotWidth] = useState(defaultSettings.robotWidth);
  const [robotLength, setRobotLength] = useState(defaultSettings.robotLength);
  const [rearTrack, setRearTrack] = useState(defaultSettings.rearTrack);

  // Movement Configuration
  const [leftTurnRadius, setLeftTurnRadius] = useState(defaultSettings.leftTurnRadius);
  const [rightTurnRadius, setRightTurnRadius] = useState(defaultSettings.rightTurnRadius);
  const [approachOffset, setApproachOffset] = useState(defaultSettings.approachOffset);
  const [safetyBuffer, setSafetyBuffer] = useState(defaultSettings.safetyBuffer);
  const [showGrid, setShowGrid] = useState(true);

  // Core State
  const [obstacles, setObstacles] = useState<ObstacleSource[]>([
    { id: 1, x: 50, y: 50, direction: 'E' },
    { id: 2, x: 150, y: 150, direction: 'W' },
    { id: 3, x: 50, y: 150, direction: 'S' },
  ]);
  const [isSimulating, setIsSimulating] = useState(false);
  const [isReading, setIsReading] = useState(false);
  const [currentFrame, setCurrentFrame] = useState(0);
  const [fullSamples, setFullSamples] = useState<{ pose: Point; isAtGoal: boolean }[]>([]);
  const [instructions, setInstructions] = useState<Command[]>([]);
  const [isCalculating, setIsCalculating] = useState(false);
  const [showInstructions, setShowInstructions] = useState(false);
  const [showImportModal, setShowImportModal] = useState(false);
  const [importJson, setImportJson] = useState('');
  const [importError, setImportError] = useState<string | null>(null);
  const [unreachableIndices, setUnreachableIndices] = useState<number[]>([]);
  const [pathOrder, setPathOrder] = useState<number[]>([]);
  const [apiInputUrl, setApiInputUrl] = useState('http://localhost:8000/obstacles');
  const [apiOutputUrl, setApiOutputUrl] = useState('http://localhost:8000/algo-process');
  const [apiStatus, setApiStatus] = useState<string | null>(null);
  const [isSyncingApi, setIsSyncingApi] = useState(false);
  const [isSendingApi, setIsSendingApi] = useState(false);
  const [apiLiveSync, setApiLiveSync] = useState(false);
  const [apiAutomaticMode, setApiAutomaticMode] = useState(false);
  const [apiPollSeconds, setApiPollSeconds] = useState(2);
  const [promptBeforeSend, setPromptBeforeSend] = useState(true);
  const apiSyncLockRef = useRef(false);
  const apiSendLockRef = useRef(false);
  const apiAutomaticLockRef = useRef(false);
  const lastAutomaticPayloadRef = useRef('');
  const lastApiSnapshotRef = useRef('');
  const calculateRunIdRef = useRef(0);

  const commitDefaults = useCallback(() => {
    const nextDefaults = sanitizeDefaultSettings({
      plannerMode,
      robotWidth,
      robotLength,
      rearTrack,
      leftTurnRadius,
      rightTurnRadius,
      approachOffset,
      safetyBuffer
    });
    setDefaultSettings(nextDefaults);
    saveDefaults(nextDefaults);
  }, [plannerMode, robotWidth, robotLength, rearTrack, leftTurnRadius, rightTurnRadius, approachOffset, safetyBuffer]);

  const resetToDefaults = useCallback(() => {
    setPlannerMode(defaultSettings.plannerMode);
    setRobotWidth(defaultSettings.robotWidth);
    setRobotLength(defaultSettings.robotLength);
    setRearTrack(defaultSettings.rearTrack);
    setLeftTurnRadius(defaultSettings.leftTurnRadius);
    setRightTurnRadius(defaultSettings.rightTurnRadius);
    setApproachOffset(defaultSettings.approachOffset);
    setSafetyBuffer(defaultSettings.safetyBuffer);
  }, [defaultSettings]);

  // Derived Goals from Obstacles (Approach Point)
  const goals = useMemo<Point[]>(() => {
    return plannerMode === 'realistic'
      ? buildGoalsFromObstaclesRealistic(obstacles, approachOffset, robotLength, robotWidth, rearTrack)
      : buildGoalsFromObstacles(obstacles, approachOffset, robotLength, robotWidth);
  }, [obstacles, plannerMode, approachOffset, robotLength, robotWidth, rearTrack]);

  // Derived Collision Rects
  const rectObstacles = useMemo<Rect[]>(() => {
    return buildRectObstacles(obstacles, safetyBuffer);
  }, [obstacles, safetyBuffer]);

  const computeRealisticPlanFromBackend = useCallback(async (
    sourceObstacles: ObstacleSource[]
  ): Promise<PathPlanResult> => {
    const endpoint = apiInputUrl.trim();
    if (!endpoint) throw new Error('Set an input API URL for realistic planning.');

    const payload = {
      auto: true,
      algorithm: 'realistic',
      left_turn_radius: leftTurnRadius,
      right_turn_radius: rightTurnRadius,
      rear_track: rearTrack,
      approach_offset: approachOffset,
      safety_buffer: safetyBuffer,
      vehicle_length: robotLength,
      vehicle_width: robotWidth,
      data: sourceObstacles.map(o => ({
        id: o.id,
        x: o.x,
        y: o.y,
        d: o.direction
      }))
    };

    const response = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    if (!response.ok) {
      const body = await response.text();
      throw new Error(`Realistic planner API ${response.status}: ${body || response.statusText}`);
    }

    const data = await response.json();
    const instructionLines = Array.isArray(data.instructions)
      ? data.instructions.filter((item: unknown): item is string => typeof item === 'string')
      : [];
    const commands = instructionStringsToCommands(instructionLines);
    const samples = buildSamplesFromRealisticCommands(
      commands,
      robotLength,
      robotWidth,
      leftTurnRadius,
      rightTurnRadius,
      rearTrack
    );

    const visitingIds = Array.isArray(data.visiting)
      ? data.visiting.map((item: unknown) => String(item))
      : [];
    const visitedIndices = new Set<number>();
    const pathOrder: number[] = [0];
    visitingIds.forEach(id => {
      const idx = sourceObstacles.findIndex(o => String(o.id) === id);
      if (idx >= 0) {
        visitedIndices.add(idx);
        pathOrder.push(idx + 1);
      }
    });

    const unreachableIndices = sourceObstacles
      .map((_, idx) => idx)
      .filter(idx => !visitedIndices.has(idx));

    return {
      pathOrder,
      samples,
      commands,
      unreachableIndices
    };
  }, [
    apiInputUrl,
    leftTurnRadius,
    rightTurnRadius,
    rearTrack,
    approachOffset,
    safetyBuffer,
    robotLength,
    robotWidth
  ]);

  // Path Generation
  const calculatePath = useCallback(() => {
    const runId = ++calculateRunIdRef.current;
    setIsCalculating(true);
    setIsSimulating(false);
    setIsReading(false);
    setCurrentFrame(0);
    setPathOrder([]);
    setFullSamples([]);
    setInstructions([]);
    setUnreachableIndices([]);
    
    setTimeout(() => {
      void (async () => {
        try {
          const plan = plannerMode === 'realistic'
            ? await computeRealisticPlanFromBackend(obstacles)
            : computePathPlan(
                obstacles,
                leftTurnRadius,
                rightTurnRadius,
                approachOffset,
                robotLength,
                robotWidth,
                safetyBuffer
              );
          if (runId !== calculateRunIdRef.current) {
            return;
          }
          setPathOrder(plan.pathOrder);
          setFullSamples(plan.samples);
          setInstructions(plan.commands);
          setUnreachableIndices(plan.unreachableIndices);
          setCurrentFrame(0);
        } catch (err) {
          if (runId === calculateRunIdRef.current) {
            console.error("Path planning error:", err);
          }
        } finally {
          if (runId === calculateRunIdRef.current) {
            setIsCalculating(false);
          }
        }
      })();
    }, 50);
  }, [
    obstacles,
    plannerMode,
    leftTurnRadius,
    rightTurnRadius,
    approachOffset,
    robotLength,
    robotWidth,
    rearTrack,
    safetyBuffer,
    computeRealisticPlanFromBackend
  ]);

  useEffect(() => {
    calculatePath();
  }, [calculatePath]);

  const automaticSettingsUrl = useMemo(() => {
    const endpoint = apiInputUrl.trim();
    if (!endpoint) return null;
    try {
      const url = new URL(endpoint);
      url.pathname = '/settings/automatic';
      url.search = '';
      url.hash = '';
      return url.toString();
    } catch {
      return null;
    }
  }, [apiInputUrl]);

  const updateAutomaticMode = useCallback(async (enabled: boolean, silent = false): Promise<boolean> => {
    if (!automaticSettingsUrl) {
      if (!silent) setApiStatus('Set a valid input API URL before toggling Automatic mode.');
      return false;
    }
    if (apiAutomaticLockRef.current) return false;

    const payload = {
      automatic: enabled,
      algorithm: plannerMode,
      radius: getEffectiveTurnRadius(leftTurnRadius, rightTurnRadius),
      left_turn_radius: leftTurnRadius,
      right_turn_radius: rightTurnRadius,
      rear_track: rearTrack,
      approach_offset: approachOffset,
      safety_buffer: safetyBuffer,
      vehicle_length: robotLength,
      vehicle_width: robotWidth
    };
    const payloadKey = JSON.stringify(payload);
    if (silent && payloadKey === lastAutomaticPayloadRef.current) {
      return true;
    }

    apiAutomaticLockRef.current = true;
    try {
      const response = await fetch(automaticSettingsUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: payloadKey
      });

      if (!response.ok) {
        const body = await response.text();
        throw new Error(`Automatic API ${response.status}: ${body || response.statusText}`);
      }

      const data = await response.json();
      const serverAutomatic = Boolean(data.automatic);
      lastAutomaticPayloadRef.current = payloadKey;
      setApiAutomaticMode(serverAutomatic);
      if (!silent) {
        setApiStatus(
          serverAutomatic
            ? 'Automatic mode enabled: POST /obstacles returns instructions immediately.'
            : 'Automatic mode disabled: POST /obstacles stores waypoints only.'
        );
      }
      return true;
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown error';
      setApiStatus(`Failed to update Automatic mode: ${message}`);
      return false;
    } finally {
      apiAutomaticLockRef.current = false;
    }
  }, [
    automaticSettingsUrl,
    plannerMode,
    leftTurnRadius,
    rightTurnRadius,
    rearTrack,
    approachOffset,
    robotLength,
    robotWidth,
    safetyBuffer
  ]);

  useEffect(() => {
    if (!automaticSettingsUrl) return;
    let cancelled = false;

    const loadAutomaticMode = async () => {
      try {
        const response = await fetch(automaticSettingsUrl);
        if (!response.ok) return;
        const data = await response.json();
        if (cancelled) return;

        if (typeof data.automatic === 'boolean') {
          setApiAutomaticMode(data.automatic);
        }
        if (data.algorithm === 'legacy' || data.algorithm === 'realistic') {
          setPlannerMode(data.algorithm);
        }
        if (Number.isFinite(Number(data.left_turn_radius))) {
          setLeftTurnRadius(clampNumber(Number(data.left_turn_radius), 4, 40));
        }
        if (Number.isFinite(Number(data.right_turn_radius))) {
          setRightTurnRadius(clampNumber(Number(data.right_turn_radius), 4, 40));
        }
        if (Number.isFinite(Number(data.rear_track))) {
          setRearTrack(clampNumber(Number(data.rear_track), 10, 40));
        }
        if (Number.isFinite(Number(data.approach_offset))) {
          setApproachOffset(clampNumber(Number(data.approach_offset), 10, 180));
        }
        if (Number.isFinite(Number(data.safety_buffer))) {
          setSafetyBuffer(clampNumber(Number(data.safety_buffer), 0, 20));
        }
        if (Number.isFinite(Number(data.vehicle_length))) {
          setRobotLength(clampNumber(Number(data.vehicle_length), 10, 60));
        }
        if (Number.isFinite(Number(data.vehicle_width))) {
          setRobotWidth(clampNumber(Number(data.vehicle_width), 10, 60));
        }
      } catch {
        // Ignore passive status load failures.
      }
    };

    void loadAutomaticMode();
    return () => {
      cancelled = true;
    };
  }, [automaticSettingsUrl]);

  useEffect(() => {
    if (!automaticSettingsUrl) return;
    const timer = window.setTimeout(() => {
      void updateAutomaticMode(apiAutomaticMode, true);
    }, 250);
    return () => window.clearTimeout(timer);
  }, [
    automaticSettingsUrl,
    apiAutomaticMode,
    plannerMode,
    leftTurnRadius,
    rightTurnRadius,
    rearTrack,
    approachOffset,
    safetyBuffer,
    robotLength,
    robotWidth,
    updateAutomaticMode
  ]);

  const sendInstructionsToApi = useCallback(async (
    sourceCommands: Command[],
    sourceObstacles: ObstacleSource[],
    shouldPrompt: boolean
  ): Promise<boolean> => {
    const endpoint = apiOutputUrl.trim();
    if (!endpoint) {
      setApiStatus('Set an output API URL before sending instructions.');
      return false;
    }
    if (sourceCommands.length === 0) {
      setApiStatus('No instructions available to send.');
      return false;
    }
    if (shouldPrompt) {
      const accepted = window.confirm(
        `Send ${sourceCommands.length} instructions to robot endpoint?\n${endpoint}`
      );
      if (!accepted) {
        setApiStatus('Instruction send cancelled.');
        return false;
      }
    }
    if (apiSendLockRef.current) return false;

    apiSendLockRef.current = true;
    setIsSendingApi(true);
    setApiStatus(`Sending ${sourceCommands.length} instructions...`);

    try {
      const payload = {
        obstacles: sourceObstacles.map(o => ({
          id: o.id,
          x: o.x,
          y: o.y,
          d: o.direction
        })),
        instructions: toInstructionStrings(sourceCommands),
        commands: sourceCommands,
        generatedAt: new Date().toISOString()
      };

      const resolveAlgoProcessMirror = (): string | null => {
        const baseInput = apiInputUrl.trim();
        if (!baseInput) return null;
        try {
          const url = new URL(baseInput);
          url.pathname = '/algo-process';
          url.search = '';
          url.hash = '';
          return url.toString();
        } catch {
          return null;
        }
      };

      const normalize = (raw: string): string => {
        try {
          const url = new URL(raw);
          url.hash = '';
          const path = url.pathname.endsWith('/') && url.pathname !== '/'
            ? url.pathname.slice(0, -1)
            : url.pathname;
          url.pathname = path;
          return url.toString();
        } catch {
          return raw;
        }
      };

      const response = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });

      if (!response.ok) {
        const body = await response.text();
        throw new Error(`Output API ${response.status}: ${body || response.statusText}`);
      }

      const mirrorEndpoint = resolveAlgoProcessMirror();
      if (mirrorEndpoint && normalize(mirrorEndpoint) !== normalize(endpoint)) {
        const mirrorResponse = await fetch(mirrorEndpoint, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        if (!mirrorResponse.ok) {
          const mirrorBody = await mirrorResponse.text();
          throw new Error(`Algo-process mirror ${mirrorResponse.status}: ${mirrorBody || mirrorResponse.statusText}`);
        }
      }

      setApiStatus(`Sent ${sourceCommands.length} instructions to robot endpoint.`);
      return true;
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown error';
      setApiStatus(`Failed to send instructions: ${message}`);
      return false;
    } finally {
      apiSendLockRef.current = false;
      setIsSendingApi(false);
    }
  }, [apiInputUrl, apiOutputUrl]);

  const syncFromApi = useCallback(async (force = false) => {
    const endpoint = apiInputUrl.trim();
    if (!endpoint) {
      setApiStatus('Set an input API URL before syncing.');
      return;
    }
    if (apiSyncLockRef.current) return;

    apiSyncLockRef.current = true;
    setIsSyncingApi(true);
    setApiStatus(`Fetching obstacle data from ${endpoint}...`);

    try {
      const response = await fetch(endpoint);
      if (!response.ok) {
        const body = await response.text();
        throw new Error(`Input API ${response.status}: ${body || response.statusText}`);
      }

      const payload = await response.json();
      const nextObstacles = parseObstaclePayload(payload);
      const snapshot = JSON.stringify(nextObstacles);
      if (!force && snapshot === lastApiSnapshotRef.current) {
        setApiStatus('No waypoint changes from input API.');
        return;
      }
      lastApiSnapshotRef.current = snapshot;
      const plan = plannerMode === 'realistic'
        ? await computeRealisticPlanFromBackend(nextObstacles)
        : computePathPlan(
            nextObstacles,
            leftTurnRadius,
            rightTurnRadius,
            approachOffset,
            robotLength,
            robotWidth,
            safetyBuffer
          );

      setObstacles(nextObstacles);
      setPathOrder(plan.pathOrder);
      setFullSamples(plan.samples);
      setInstructions(plan.commands);
      setUnreachableIndices(plan.unreachableIndices);
      setCurrentFrame(0);
      setIsSimulating(false);
      setIsReading(false);
      setApiStatus(
        `Loaded ${nextObstacles.length} waypoints and generated ${plan.commands.length} commands.`
      );

      if (plan.commands.length > 0) {
        await sendInstructionsToApi(plan.commands, nextObstacles, promptBeforeSend);
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown error';
      setApiStatus(`Sync failed: ${message}`);
    } finally {
      apiSyncLockRef.current = false;
      setIsSyncingApi(false);
    }
  }, [
    apiInputUrl,
    plannerMode,
    leftTurnRadius,
    rightTurnRadius,
    approachOffset,
    robotLength,
    robotWidth,
    rearTrack,
    safetyBuffer,
    computeRealisticPlanFromBackend,
    sendInstructionsToApi,
    promptBeforeSend
  ]);

  const sendCurrentInstructions = useCallback(async () => {
    await sendInstructionsToApi(instructions, obstacles, promptBeforeSend);
  }, [instructions, obstacles, sendInstructionsToApi, promptBeforeSend]);

  useEffect(() => {
    if (!apiLiveSync) return;
    const pollMs = Math.max(1, apiPollSeconds) * 1000;
    const interval = window.setInterval(() => {
      void syncFromApi(false);
    }, pollMs);
    return () => clearInterval(interval);
  }, [apiLiveSync, apiPollSeconds, syncFromApi]);

  // Simulation Loop
  useEffect(() => {
    let interval: number | undefined;
    if (isSimulating && !isReading && fullSamples.length > 0) {
      interval = window.setInterval(() => {
        setCurrentFrame(prev => {
          const next = prev + 1;
          if (next >= fullSamples.length) {
            setIsSimulating(false);
            return prev;
          }
          if (fullSamples[next].isAtGoal) {
            setIsReading(true);
            setTimeout(() => setIsReading(false), 500);
          }
          return next;
        });
      }, 30);
    }
    return () => clearInterval(interval);
  }, [isSimulating, isReading, fullSamples]);

  const instructionStrings = useMemo(() => {
    return toInstructionStrings(instructions);
  }, [instructions]);

  // Download logic
  const downloadFile = (data: any, filename: string) => {
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  const downloadInstructions = () => {
    downloadFile(instructionStrings, 'robot_commands.json');
  };

  const downloadWorkspace = () => {
    const workspace = {
      algorithm: plannerMode,
      robot: { width: robotWidth, length: robotLength },
      movement: {
        rearTrack,
        leftTurnRadius,
        rightTurnRadius,
        radius: getEffectiveTurnRadius(leftTurnRadius, rightTurnRadius),
        approachOffset,
        safetyBuffer
      },
      obstacles: obstacles.map(o => ({ id: o.id, x: o.x, y: o.y, d: o.direction }))
    };
    downloadFile(workspace, 'robot_simulator_workspace.json');
  };

  // Handlers
  const handleImport = () => {
    try {
      setImportError(null);
      const data = JSON.parse(importJson);
      const mapped = parseObstaclePayload(data);

      if (!Array.isArray(data) && data.robot) {
        const nextRobotWidth = Number(data.robot.width);
        const nextRobotLength = Number(data.robot.length);
        if (Number.isFinite(nextRobotWidth)) setRobotWidth(clampNumber(nextRobotWidth, 10, 60));
        if (Number.isFinite(nextRobotLength)) setRobotLength(clampNumber(nextRobotLength, 10, 60));
      }
      if (!Array.isArray(data) && (data.algorithm === 'legacy' || data.algorithm === 'realistic')) {
        setPlannerMode(data.algorithm);
      }
      if (!Array.isArray(data) && data.movement) {
        const movement = data.movement;
        if (movement.algorithm === 'legacy' || movement.algorithm === 'realistic') {
          setPlannerMode(movement.algorithm);
        }
        const legacyRadius = Number(movement.radius);
        const nextRearTrack = Number(movement.rearTrack ?? movement.rear_track);
        const nextLeft = Number(movement.leftTurnRadius);
        const nextRight = Number(movement.rightTurnRadius);
        const left = Number.isFinite(nextLeft)
          ? nextLeft
          : Number.isFinite(legacyRadius)
            ? legacyRadius
            : defaultSettings.leftTurnRadius;
        const right = Number.isFinite(nextRight)
          ? nextRight
          : Number.isFinite(legacyRadius)
            ? legacyRadius
            : defaultSettings.rightTurnRadius;
        setLeftTurnRadius(clampNumber(left, 4, 40));
        setRightTurnRadius(clampNumber(right, 4, 40));
        if (Number.isFinite(nextRearTrack)) {
          setRearTrack(clampNumber(nextRearTrack, 10, 40));
        }

        const nextApproach = Number(movement.approachOffset);
        const nextSafety = Number(movement.safetyBuffer);
        if (Number.isFinite(nextApproach)) setApproachOffset(clampNumber(nextApproach, 10, 180));
        if (Number.isFinite(nextSafety)) setSafetyBuffer(clampNumber(nextSafety, 0, 20));
      }

      setObstacles(mapped);
      setShowImportModal(false);
      setImportJson('');
    } catch (e: any) {
      setImportError(e.message);
    }
  };

  const openImportModal = () => {
    const currentJson = JSON.stringify(
      obstacles.map(o => ({ id: o.id, x: o.x, y: o.y, d: o.direction })),
      null, 
      2
    );
    setImportJson(currentJson);
    setShowImportModal(true);
  };

  const addObstacle = () => {
    const nextId = Math.max(0, ...obstacles.map(o => (typeof o.id === 'number' ? o.id : 0))) + 1;
    setObstacles([...obstacles, { id: nextId, x: 100, y: 100, direction: 'E' }]);
  };

  const updateObstacle = (index: number, field: keyof ObstacleSource, value: any) => {
    const updated = [...obstacles];
    updated[index] = { ...updated[index], [field]: value };
    setObstacles(updated);
  };

  const removeObstacle = (index: number) => {
    setObstacles(obstacles.filter((_, i) => i !== index));
  };

  const resetSimulation = () => {
    setCurrentFrame(0);
    setIsSimulating(false);
    setIsReading(false);
  };

  const currentPose = fullSamples[currentFrame]?.pose || goals[0];
  const [poseX, poseY, poseTheta] = currentPose;
  const headingX = Math.cos(poseTheta);
  const headingY = Math.sin(poseTheta);
  const leftX = -Math.sin(poseTheta);
  const leftY = Math.cos(poseTheta);
  const rightX = -leftX;
  const rightY = -leftY;
  const halfWidth = robotWidth / 2;
  const halfLength = robotLength / 2;

  let rearLeftX = poseX;
  let rearLeftY = poseY;
  let rearRightX = poseX + rightX * robotWidth;
  let rearRightY = poseY + rightY * robotWidth;
  let frontLeftX = poseX + headingX * robotLength;
  let frontLeftY = poseY + headingY * robotLength;
  let frontRightX = frontLeftX + rightX * robotWidth;
  let frontRightY = frontLeftY + rightY * robotWidth;
  let frontCenterX = (frontLeftX + frontRightX) / 2;
  let frontCenterY = (frontLeftY + frontRightY) / 2;
  let rearLeftWheelX = rearLeftX;
  let rearLeftWheelY = rearLeftY;
  let rearRightWheelX = rearRightX;
  let rearRightWheelY = rearRightY;

  if (plannerMode === 'realistic') {
    const rearCenterX = poseX - headingX * halfLength;
    const rearCenterY = poseY - headingY * halfLength;
    frontCenterX = poseX + headingX * halfLength;
    frontCenterY = poseY + headingY * halfLength;

    rearLeftX = rearCenterX - rightX * halfWidth;
    rearLeftY = rearCenterY - rightY * halfWidth;
    rearRightX = rearCenterX + rightX * halfWidth;
    rearRightY = rearCenterY + rightY * halfWidth;
    frontLeftX = frontCenterX - rightX * halfWidth;
    frontLeftY = frontCenterY - rightY * halfWidth;
    frontRightX = frontCenterX + rightX * halfWidth;
    frontRightY = frontCenterY + rightY * halfWidth;

    const wheelHalfTrack = rearTrack / 2;
    rearLeftWheelX = rearCenterX - rightX * wheelHalfTrack;
    rearLeftWheelY = rearCenterY - rightY * wheelHalfTrack;
    rearRightWheelX = rearCenterX + rightX * wheelHalfTrack;
    rearRightWheelY = rearCenterY + rightY * wheelHalfTrack;
  }

  const arrowTipX = frontCenterX + headingX * (robotLength * 0.12);
  const arrowTipY = frontCenterY + headingY * (robotLength * 0.12);
  const arrowBaseCenterX = frontCenterX - headingX * (robotLength * 0.08);
  const arrowBaseCenterY = frontCenterY - headingY * (robotLength * 0.08);
  const arrowHalfWidth = robotWidth * 0.16;
  const arrowLeftX = arrowBaseCenterX + leftX * arrowHalfWidth;
  const arrowLeftY = arrowBaseCenterY + leftY * arrowHalfWidth;
  const arrowRightX = arrowBaseCenterX - leftX * arrowHalfWidth;
  const arrowRightY = arrowBaseCenterY - leftY * arrowHalfWidth;

  const gridLines = useMemo(() => {
    const lines = [];
    for (let i = 10; i < X_MAX; i += 10) {
      lines.push(<line key={`vx-${i}`} x1={i} y1={0} x2={i} y2={Y_MAX} stroke="#1e293b" strokeWidth="0.2" />);
      lines.push(<line key={`hy-${i}`} x1={0} y1={i} x2={X_MAX} y2={i} stroke="#1e293b" strokeWidth="0.2" />);
    }
    for (let i = 50; i < X_MAX; i += 50) {
      lines.push(<line key={`mvx-${i}`} x1={i} y1={0} x2={i} y2={Y_MAX} stroke="#334155" strokeWidth="0.4" />);
      lines.push(<line key={`mhy-${i}`} x1={0} y1={i} x2={X_MAX} y2={i} stroke="#334155" strokeWidth="0.4" />);
    }
    return lines;
  }, []);

  return (
    <div className="flex h-screen bg-slate-950 overflow-hidden text-slate-200 font-sans">
      {/* Sidebar */}
      <div className="w-96 bg-slate-900 border-r border-slate-800 flex flex-col shadow-2xl z-20 overflow-hidden">
        <div className="p-6 border-b border-slate-800 bg-slate-900/50">
          <h1 className="text-xl font-bold flex items-center gap-2 text-indigo-400">
            <Navigation className="w-6 h-6" />
            Robot Simulator
          </h1>
          <p className="text-[10px] text-slate-500 mt-1 uppercase tracking-[0.2em] font-black underline decoration-indigo-500/50">
            {plannerMode === 'realistic' ? 'realistic wheel-based planner' : "dubin's path algorithm"}
          </p>
          <div className="mt-4 flex gap-2">
            <button
              onClick={resetToDefaults}
              className="flex-1 py-1.5 rounded-lg border border-slate-700 bg-slate-800 text-slate-300 text-[10px] font-bold uppercase tracking-wide hover:bg-slate-700"
            >
              Reset To Default
            </button>
            <button
              onClick={commitDefaults}
              className="flex-1 py-1.5 rounded-lg border border-indigo-500/30 bg-indigo-600/20 text-indigo-300 text-[10px] font-bold uppercase tracking-wide hover:bg-indigo-600/30"
            >
              Set As Default
            </button>
          </div>
        </div>

        <div className="flex-1 overflow-y-auto p-4 space-y-6 scrollbar-thin scrollbar-thumb-slate-700">
          
          {/* Robot Configuration */}
          <section>
            <h2 className="text-xs font-bold uppercase tracking-widest text-slate-500 flex items-center gap-2 mb-4">
              <Cpu className="w-4 h-4" /> Robot Configuration (cm)
            </h2>
            <div className="bg-slate-800/30 p-4 rounded-xl border border-slate-800 space-y-5">
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Width</span>
                  <span className="text-indigo-400">{robotWidth}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="10"
                    max="60"
                    step="1"
                    value={robotWidth}
                    onChange={e => setRobotWidth(clampNumber(Number(e.target.value), 10, 60))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
                  />
                  <input
                    type="number"
                    min="10"
                    max="60"
                    step="1"
                    value={robotWidth}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setRobotWidth(clampNumber(value, 10, 60));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-indigo-300"
                  />
                </div>
              </div>
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Length</span>
                  <span className="text-indigo-400">{robotLength}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="10"
                    max="60"
                    step="1"
                    value={robotLength}
                    onChange={e => setRobotLength(clampNumber(Number(e.target.value), 10, 60))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
                  />
                  <input
                    type="number"
                    min="10"
                    max="60"
                    step="1"
                    value={robotLength}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setRobotLength(clampNumber(value, 10, 60));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-indigo-300"
                  />
                </div>
              </div>
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Rear Wheel Track</span>
                  <span className="text-indigo-400">{rearTrack}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="10"
                    max="40"
                    step="1"
                    value={rearTrack}
                    onChange={e => setRearTrack(clampNumber(Number(e.target.value), 10, 40))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
                  />
                  <input
                    type="number"
                    min="10"
                    max="40"
                    step="1"
                    value={rearTrack}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setRearTrack(clampNumber(value, 10, 40));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-indigo-300"
                  />
                </div>
              </div>
            </div>
          </section>

          {/* Movement Configuration */}
          <section>
            <h2 className="text-xs font-bold uppercase tracking-widest text-slate-500 flex items-center gap-2 mb-4">
              <Move className="w-4 h-4" /> Movement Configuration (cm)
            </h2>
            <div className="bg-slate-800/30 p-4 rounded-xl border border-slate-800 space-y-5">
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Planner Mode</span>
                  <span className="text-cyan-400 uppercase">{plannerMode}</span>
                </div>
                <div className="grid grid-cols-2 gap-2">
                  <button
                    onClick={() => setPlannerMode('legacy')}
                    className={`py-2 rounded-lg border text-[10px] font-bold uppercase tracking-wide ${
                      plannerMode === 'legacy'
                        ? 'bg-indigo-600/20 border-indigo-500/40 text-indigo-300'
                        : 'bg-slate-900 border-slate-700 text-slate-400 hover:border-slate-500'
                    }`}
                  >
                    Legacy
                  </button>
                  <button
                    onClick={() => setPlannerMode('realistic')}
                    className={`py-2 rounded-lg border text-[10px] font-bold uppercase tracking-wide ${
                      plannerMode === 'realistic'
                        ? 'bg-cyan-600/20 border-cyan-500/40 text-cyan-300'
                        : 'bg-slate-900 border-slate-700 text-slate-400 hover:border-slate-500'
                    }`}
                  >
                    Realistic
                  </button>
                </div>
              </div>
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Left Turn Radius</span>
                  <span className="text-indigo-400">{leftTurnRadius}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="4"
                    max="40"
                    step="0.5"
                    value={leftTurnRadius}
                    onChange={e => setLeftTurnRadius(clampNumber(Number(e.target.value), 4, 40))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
                  />
                  <input
                    type="number"
                    min="4"
                    max="40"
                    step="0.5"
                    value={leftTurnRadius}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setLeftTurnRadius(clampNumber(value, 4, 40));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-indigo-300"
                  />
                </div>
              </div>
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Right Turn Radius</span>
                  <span className="text-indigo-400">{rightTurnRadius}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="4"
                    max="40"
                    step="0.5"
                    value={rightTurnRadius}
                    onChange={e => setRightTurnRadius(clampNumber(Number(e.target.value), 4, 40))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
                  />
                  <input
                    type="number"
                    min="4"
                    max="40"
                    step="0.5"
                    value={rightTurnRadius}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setRightTurnRadius(clampNumber(value, 4, 40));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-indigo-300"
                  />
                </div>
              </div>
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">Reading Distance</span>
                  <span className="text-indigo-400">{approachOffset}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="10"
                    max="180"
                    step="1"
                    value={approachOffset}
                    onChange={e => setApproachOffset(clampNumber(Number(e.target.value), 10, 180))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
                  />
                  <input
                    type="number"
                    min="10"
                    max="180"
                    step="1"
                    value={approachOffset}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setApproachOffset(clampNumber(value, 10, 180));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-indigo-300"
                  />
                </div>
              </div>
              <div>
                <div className="flex justify-between text-[10px] mb-2 font-mono">
                  <span className="text-slate-500 uppercase">No-Enter Buffer</span>
                  <span className="text-rose-400">+{safetyBuffer}cm</span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="range"
                    min="0"
                    max="20"
                    step="1"
                    value={safetyBuffer}
                    onChange={e => setSafetyBuffer(clampNumber(Number(e.target.value), 0, 20))}
                    className="flex-1 h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-rose-500"
                  />
                  <input
                    type="number"
                    min="0"
                    max="20"
                    step="1"
                    value={safetyBuffer}
                    onChange={e => {
                      const value = Number(e.target.value);
                      if (Number.isFinite(value)) setSafetyBuffer(clampNumber(value, 0, 20));
                    }}
                    className="w-20 bg-slate-900 border border-slate-700 rounded-lg px-2 py-1 text-xs font-mono text-rose-300"
                  />
                </div>
              </div>
              <div className="flex items-center justify-between pt-2 border-t border-slate-800/50">
                 <span className="text-[10px] text-slate-500 font-bold uppercase">Grid Overlay</span>
                 <button onClick={() => setShowGrid(!showGrid)} className={`p-1.5 rounded-lg transition-colors ${showGrid ? 'bg-indigo-500/20 text-indigo-400' : 'bg-slate-800 text-slate-600'}`}>
                    <Grid3X3 className="w-4 h-4" />
                 </button>
              </div>
            </div>
          </section>

          {/* API Bridge */}
          <section>
            <h2 className="text-xs font-bold uppercase tracking-widest text-slate-500 flex items-center gap-2 mb-4">
              <Settings2 className="w-4 h-4" /> API Bridge
            </h2>
            <div className="bg-slate-800/30 p-4 rounded-xl border border-slate-800 space-y-3">
              <div className="space-y-1">
                <label className="text-[9px] text-slate-600 font-bold uppercase">Input API URL</label>
                <input
                  type="text"
                  value={apiInputUrl}
                  onChange={e => setApiInputUrl(e.target.value)}
                  placeholder="http://localhost:8000/obstacles"
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-2 py-2 text-xs font-mono"
                />
              </div>
              <div className="space-y-1">
                <label className="text-[9px] text-slate-600 font-bold uppercase">Output API URL</label>
                <input
                  type="text"
                  value={apiOutputUrl}
                  onChange={e => setApiOutputUrl(e.target.value)}
                  placeholder="http://localhost:8000/algo-process"
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-2 py-2 text-xs font-mono"
                />
              </div>

              <div className="grid grid-cols-2 gap-2">
                <button
                  onClick={() => void syncFromApi(true)}
                  disabled={isSyncingApi}
                  className="py-2 bg-indigo-600/10 hover:bg-indigo-600/20 disabled:opacity-50 text-indigo-400 rounded-lg text-xs font-bold border border-indigo-500/20"
                >
                  {isSyncingApi ? 'SYNCING...' : 'SYNC + PLAN'}
                </button>
                <button
                  onClick={() => void sendCurrentInstructions()}
                  disabled={isSendingApi || instructions.length === 0}
                  className="py-2 bg-emerald-600/10 hover:bg-emerald-600/20 disabled:opacity-50 text-emerald-400 rounded-lg text-xs font-bold border border-emerald-500/20"
                >
                  {isSendingApi ? 'SENDING...' : 'SEND INSTRUCTIONS'}
                </button>
              </div>

              <div className="grid grid-cols-3 gap-2 pt-1">
                <label className="flex items-center justify-between px-2 py-1.5 rounded-lg border border-slate-700 bg-slate-900 text-[10px] text-slate-400">
                  <span>Live Sync</span>
                  <input
                    type="checkbox"
                    checked={apiLiveSync}
                    onChange={e => setApiLiveSync(e.target.checked)}
                    className="accent-indigo-500"
                  />
                </label>
                <label className="flex items-center justify-between px-2 py-1.5 rounded-lg border border-slate-700 bg-slate-900 text-[10px] text-slate-400">
                  <span>Prompt Send</span>
                  <input
                    type="checkbox"
                    checked={promptBeforeSend}
                    onChange={e => setPromptBeforeSend(e.target.checked)}
                    className="accent-emerald-500"
                  />
                </label>
                <label className="flex items-center justify-between px-2 py-1.5 rounded-lg border border-slate-700 bg-slate-900 text-[10px] text-slate-400">
                  <span>Automatic</span>
                  <input
                    type="checkbox"
                    checked={apiAutomaticMode}
                    onChange={e => {
                      const nextValue = e.target.checked;
                      setApiAutomaticMode(nextValue);
                      void (async () => {
                        const ok = await updateAutomaticMode(nextValue);
                        if (!ok) {
                          setApiAutomaticMode(!nextValue);
                        }
                      })();
                    }}
                    className="accent-cyan-500"
                  />
                </label>
              </div>

              <div className="space-y-1">
                <label className="text-[9px] text-slate-600 font-bold uppercase">Live Sync Interval (sec)</label>
                <input
                  type="number"
                  min={1}
                  value={apiPollSeconds}
                  onChange={e => setApiPollSeconds(Math.max(1, Number(e.target.value) || 1))}
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-2 py-2 text-xs font-mono"
                />
              </div>

              {apiStatus && (
                <div className="p-2 bg-slate-900 border border-slate-700 rounded-lg text-[10px] text-slate-400 font-mono">
                  {apiStatus}
                </div>
              )}
            </div>
          </section>

          {/* Sync & Obstacle Management */}
          <section className="space-y-4">
            <div className="flex items-center justify-between">
              <h2 className="text-xs font-bold uppercase tracking-widest text-slate-500 flex items-center gap-2">
                <List className="w-4 h-4" /> Waypoints
              </h2>
              <button onClick={addObstacle} className="p-1 hover:text-indigo-400 text-slate-500 transition-colors">
                <PlusCircle className="w-4 h-4" />
              </button>
            </div>
            
            <button onClick={openImportModal} className="w-full py-2.5 bg-indigo-600/10 hover:bg-indigo-600/20 text-indigo-400 rounded-xl flex items-center justify-center gap-2 text-xs font-bold transition-all border border-indigo-500/20">
              <Tablet className="w-4 h-4" /> Sync Tablet JSON
            </button>

            <div className="space-y-3">
              {obstacles.map((obs, idx) => {
                const tourStep = pathOrder.indexOf(idx + 1); 
                return (
                  <div key={idx} className={`p-3 rounded-xl border space-y-3 transition-all ${unreachableIndices.includes(idx) ? 'bg-rose-500/10 border-rose-500/30' : 'bg-slate-800/40 border-slate-800'}`}>
                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-2">
                        <span className="w-5 h-5 rounded bg-slate-700 flex items-center justify-center font-bold text-[10px]">{obs.id}</span>
                        {tourStep !== -1 && tourStep !== 0 && (
                          <span className="px-1.5 py-0.5 rounded bg-rose-500/20 text-rose-400 text-[9px] font-black uppercase tracking-tighter">Visit #{tourStep}</span>
                        )}
                      </div>
                      <button onClick={() => removeObstacle(idx)} className="text-slate-600 hover:text-rose-400 transition-colors">
                        <Trash2 className="w-3.5 h-3.5" />
                      </button>
                    </div>
                    
                    <div className="grid grid-cols-2 gap-2">
                      <div className="space-y-1">
                        <label className="text-[9px] text-slate-600 font-bold uppercase">X (cm)</label>
                        <input type="number" value={obs.x} onChange={e => updateObstacle(idx, 'x', Number(e.target.value))} className="w-full bg-slate-900 border border-slate-700 rounded-lg px-2 py-1.5 text-xs font-mono" />
                      </div>
                      <div className="space-y-1">
                        <label className="text-[9px] text-slate-600 font-bold uppercase">Y (cm)</label>
                        <input type="number" value={obs.y} onChange={e => updateObstacle(idx, 'y', Number(e.target.value))} className="w-full bg-slate-900 border border-slate-700 rounded-lg px-2 py-1.5 text-xs font-mono" />
                      </div>
                    </div>

                    <div className="flex gap-1 pt-1">
                      {(['N', 'S', 'E', 'W'] as Direction[]).map(d => (
                        <button key={d} onClick={() => updateObstacle(idx, 'direction', d)} className={`flex-1 py-1 text-[9px] font-black rounded border transition-all ${obs.direction === d ? 'bg-indigo-600 border-indigo-500 text-white' : 'bg-slate-900 border-slate-700 text-slate-500 hover:border-slate-500'}`}>
                          {d}
                        </button>
                      ))}
                    </div>
                  </div>
                );
              })}
            </div>
          </section>
        </div>

        {/* Footer Actions */}
        <div className="p-6 border-t border-slate-800 bg-slate-900 space-y-3 shadow-inner">
          <div className="flex gap-2">
            <button disabled={fullSamples.length === 0} onClick={() => setIsSimulating(!isSimulating)} className={`flex-1 py-3 rounded-xl flex items-center justify-center gap-2 font-bold transition-all shadow-lg ${isSimulating ? 'bg-rose-500/10 text-rose-500 border border-rose-500/50' : 'bg-indigo-600 text-white hover:bg-indigo-500 disabled:bg-slate-800'}`}>
              {isSimulating ? <Pause className="w-5 h-5" /> : <Play className="w-5 h-5 fill-current" />}
              {isSimulating ? (isReading ? 'READING...' : 'STOP') : 'RUN TOUR'}
            </button>
            <button onClick={resetSimulation} className="p-3 bg-slate-800 text-slate-400 hover:text-white rounded-xl border border-slate-700 transition-all"><RotateCcw className="w-5 h-5" /></button>
          </div>
          <button onClick={() => setShowInstructions(true)} className="w-full py-2 bg-slate-800 hover:bg-slate-700 text-slate-400 rounded-lg flex items-center justify-center gap-2 text-xs font-bold transition-all border border-slate-700">
            <FileText className="w-4 h-4" /> Full Instruction List
          </button>
        </div>
      </div>

      {/* Main Viewport */}
      <div className="flex-1 relative bg-slate-950 flex items-center justify-center overflow-hidden p-8">
        <div className="relative w-full h-full max-w-[90vh] max-h-[90vh] bg-slate-900 rounded-2xl shadow-[0_0_80px_rgba(0,0,0,0.6)] border border-slate-800 overflow-hidden flex items-center justify-center aspect-square">
          <svg viewBox={`-5 -5 ${X_MAX + 10} ${Y_MAX + 10}`} className="w-full h-full" style={{ transform: 'scaleY(-1)' }}>
            {showGrid && gridLines}
            <rect x={0} y={0} width={X_MAX} height={Y_MAX} fill="none" stroke="#334155" strokeWidth="0.5" />

            {/* Obstacles & ID numbers */}
            {rectObstacles.map((rect, i) => {
              const unreachable = unreachableIndices.includes(i);
              const { x, y, direction, id } = obstacles[i];

              return (
                <g key={`obs-${i}`}>
                  {/* Buffer Zone */}
                  <rect 
                    x={rect[0]} y={rect[2]} width={rect[1] - rect[0]} height={rect[3] - rect[2]} 
                    fill={unreachable ? "#f43f5e" : "#fbbf24"} fillOpacity="0.05" 
                    stroke={unreachable ? "#f43f5e" : "#fbbf24"} strokeWidth="0.1" strokeDasharray="1 0.5" 
                  />
                  {/* Physical Obstacle (10x10) */}
                  <rect 
                    x={x} y={y} width="10" height="10" 
                    fill={unreachable ? "#f43f5e" : "#0f172a"} 
                    stroke={unreachable ? "#f43f5e" : "#94a3b8"} strokeWidth="0.4" rx="1" 
                  />
                  
                  {/* Target Obstacle ID Number Badge (Inside) */}
                  <g transform={`translate(${x + 5}, ${y + 5})`}>
                    <circle r="3.5" fill="#334155" opacity="0.8" />
                    <text 
                      fontSize="4" fontWeight="900" fill="white" 
                      textAnchor="middle" dominantBaseline="central" 
                      style={{ transform: `scale(1, -1)` }}
                    >{id}</text>
                  </g>

                  {/* Directional Indicator (Blue bar) */}
                  {direction === 'N' && <rect x={x} y={y + 9} width="10" height="1" fill="#6366f1" opacity="0.8" />}
                  {direction === 'S' && <rect x={x} y={y} width="10" height="1" fill="#6366f1" opacity="0.8" />}
                  {direction === 'W' && <rect x={x} y={y} width="1" height="10" fill="#6366f1" opacity="0.8" />}
                  {direction === 'E' && <rect x={x + 9} y={y} width="1" height="10" fill="#6366f1" opacity="0.8" />}
                </g>
              );
            })}

            {/* Path */}
            {!isCalculating && fullSamples.length > 0 && (
              <polyline 
                points={fullSamples.map(d => `${d.pose[0]},${d.pose[1]}`).join(' ')} 
                fill="none" stroke="#6366f1" strokeWidth="0.6" strokeLinecap="round" strokeLinejoin="round" opacity="0.4" 
              />
            )}

            {/* Approach Goals & Visit Order Labels */}
            {goals.map(([gx, gy, theta], i) => {
              const tourIdx = pathOrder.indexOf(i);
              return (
                <g key={`goal-${i}`} opacity={i > 0 && unreachableIndices.includes(i - 1) ? 0.2 : 1}>
                  <circle cx={gx} cy={gy} r="1" fill={i === 0 ? "#10b981" : "#6366f1"} stroke="white" strokeWidth="0.2" />
                  <line 
                    x1={gx} y1={gy} x2={gx + Math.cos(theta) * 8} y2={gy + Math.sin(theta) * 8} 
                    stroke={i === 0 ? "#10b981" : "#6366f1"} strokeWidth="0.5" strokeLinecap="round" 
                  />
                  {/* Visit order next to arrow in bright red (Reduced size to 3.5) */}
                  {i > 0 && tourIdx !== -1 && tourIdx !== 0 && (
                    <text 
                      x={gx + Math.cos(theta + Math.PI/2) * 5} 
                      y={gy + Math.sin(theta + Math.PI/2) * 5}
                      fontSize="3.5" fontWeight="900" fill="#f43f5e" 
                      textAnchor="middle" dominantBaseline="central"
                      style={{ transform: `scale(1, -1)`, transformOrigin: `${gx}px ${gy}px` }}
                    >#{tourIdx}</text>
                  )}
                </g>
              );
            })}

            {/* Robot */}
            {currentPose && (
              <>
                {isReading && <circle cx={frontCenterX} cy={frontCenterY} r={robotWidth * 0.4} fill="#3b82f6" fillOpacity="0.2" className="animate-pulse" />}
                <polygon
                  points={`${rearLeftX},${rearLeftY} ${rearRightX},${rearRightY} ${frontRightX},${frontRightY} ${frontLeftX},${frontLeftY}`}
                  fill="#6366f1"
                  stroke="white"
                  strokeWidth="0.4"
                />
                <circle cx={rearLeftWheelX} cy={rearLeftWheelY} r="1.1" fill="#0f172a" stroke="white" strokeWidth="0.25" />
                <circle cx={rearRightWheelX} cy={rearRightWheelY} r="1.1" fill="#0f172a" stroke="white" strokeWidth="0.25" />
                <path d={`M ${arrowTipX} ${arrowTipY} L ${arrowLeftX} ${arrowLeftY} L ${arrowRightX} ${arrowRightY} Z`} fill="white" />
              </>
            )}
          </svg>

          <div className="absolute bottom-6 right-6 bg-slate-900/90 border border-slate-700 p-3 rounded-xl backdrop-blur-md font-mono text-[10px] text-slate-500 flex items-center gap-3">
            <div className="flex flex-col">
              <span className="text-white font-bold">DIMENSIONS</span>
              <span>2.0m x 2.0m (200cm²)</span>
            </div>
          </div>
        </div>

        {isReading && (
          <div className="absolute inset-0 flex items-center justify-center bg-indigo-500/5 backdrop-blur-[1px] pointer-events-none z-10">
            <div className="bg-indigo-600 text-white px-8 py-4 rounded-2xl shadow-2xl flex flex-col items-center gap-2 animate-bounce border-2 border-white/20">
              <Eye className="w-8 h-8" />
              <span className="text-xl font-black uppercase tracking-[0.2em]">Reading target...</span>
            </div>
          </div>
        )}

        {isCalculating && (
          <div className="absolute inset-0 flex items-center justify-center bg-slate-950/50 backdrop-blur-[1px] pointer-events-none z-20">
            <div className="bg-slate-900 border border-slate-700 px-6 py-3 rounded-xl text-sm font-mono text-indigo-300">
              Calculating path...
            </div>
          </div>
        )}

        {/* Telemetry Panel */}
        <div className="absolute top-12 left-12">
          <div className="bg-slate-900/95 border border-slate-700 p-4 rounded-2xl backdrop-blur-xl shadow-2xl min-w-[160px]">
            <div className="text-[10px] text-indigo-400 font-black uppercase tracking-widest mb-3 flex items-center gap-2">
              <div className={`w-2 h-2 rounded-full ${isSimulating ? 'bg-green-500 animate-pulse' : 'bg-slate-700'}`} />
              Active Telemetry (cm)
            </div>
            <div className="grid grid-cols-2 gap-y-2 text-xs font-mono">
              <span className="text-slate-500 text-[10px]">X_POS:</span><span className="text-white text-right">{currentPose[0].toFixed(1)}</span>
              <span className="text-slate-500 text-[10px]">Y_POS:</span><span className="text-white text-right">{currentPose[1].toFixed(1)}</span>
              <span className="text-slate-500 text-[10px]">ANGLE:</span><span className="text-white text-right">{Math.round(currentPose[2] * (180/Math.PI))}°</span>
            </div>
          </div>
        </div>

        {/* Import/Export Modal */}
        {showImportModal && (
          <div className="fixed inset-0 z-50 flex items-center justify-center p-8 bg-slate-950/90 backdrop-blur-md">
            <div className="bg-slate-900 w-full max-w-xl rounded-3xl border border-slate-800 shadow-2xl flex flex-col overflow-hidden animate-in zoom-in-95 duration-200">
              <div className="p-6 border-b border-slate-800 flex items-center justify-between">
                <div><h3 className="text-xl font-bold text-slate-100 flex items-center gap-2"><Tablet className="w-6 h-6 text-indigo-400" /> Sync Tablet Data</h3></div>
                <button onClick={() => setShowImportModal(false)} className="p-2 hover:bg-slate-800 rounded-lg"><X className="w-5 h-5 text-slate-500" /></button>
              </div>
              <div className="p-6 space-y-4">
                <div className="p-4 bg-slate-950 border border-slate-800 rounded-2xl space-y-2">
                  <p className="text-[10px] text-slate-500 font-bold uppercase">Format Example:</p>
                  <pre className="text-[10px] text-indigo-400 font-mono overflow-x-auto">
{`[
  { "id": 1, "x": 50, "y": 120, "d": "N" },
  { "id": 2, "x": 1500, "y": 500, "d": "E" }
]`}
                  </pre>
                  <p className="text-[9px] text-slate-600 font-medium">* Workspace JSON (with robot settings) is also supported.</p>
                </div>

                <div className="space-y-2">
                   <div className="flex items-center justify-between">
                      <h4 className="text-[10px] font-bold uppercase tracking-widest text-slate-500">Current JSON Array</h4>
                      <button onClick={downloadWorkspace} className="text-[10px] text-indigo-400 hover:text-indigo-300 font-bold uppercase flex items-center gap-1">
                        <Download className="w-3 h-3" /> Save to File
                      </button>
                   </div>
                   <textarea 
                    value={importJson} 
                    onChange={e => setImportJson(e.target.value)} 
                    placeholder="Paste JSON array here..." 
                    className="w-full h-48 bg-slate-950 border border-slate-700 rounded-2xl p-4 font-mono text-sm focus:ring-2 focus:ring-indigo-500 outline-none resize-none text-indigo-300" 
                  />
                </div>
                
                {importError && <div className="p-3 bg-rose-500/10 border border-rose-500/20 rounded-xl text-rose-400 text-xs font-bold">{importError}</div>}
              </div>
              <div className="p-6 bg-slate-800/30 border-t border-slate-800 flex gap-3">
                <button onClick={handleImport} className="flex-1 py-3 bg-indigo-600 hover:bg-indigo-500 text-white rounded-xl font-bold flex items-center justify-center gap-2 shadow-lg shadow-indigo-600/20"><CheckCircle2 className="w-5 h-5" /> Import Data</button>
              </div>
            </div>
          </div>
        )}

        {/* Script Modal */}
        {showInstructions && (
          <div className="fixed inset-0 z-50 flex items-center justify-center p-8 bg-slate-950/80 backdrop-blur-sm">
            <div className="bg-slate-900 w-full max-w-2xl h-[80vh] rounded-3xl border border-slate-800 shadow-2xl flex flex-col overflow-hidden animate-in zoom-in-95 duration-200">
              <div className="p-6 border-b border-slate-800 flex items-center justify-between">
                <div className="flex flex-col">
                   <h3 className="text-xl font-bold text-slate-100 flex items-center gap-2"><FileText className="w-6 h-6 text-indigo-400" /> Compiled Path Instructions</h3>
                   <span className="text-[10px] text-slate-500 uppercase tracking-widest mt-1">Numerical output • No unit suffixes</span>
                </div>
                <div className="flex items-center gap-3">
                   <button onClick={downloadInstructions} className="p-2.5 bg-indigo-600 hover:bg-indigo-500 rounded-xl transition-all flex items-center gap-2 text-xs font-bold text-white shadow-lg shadow-indigo-600/20">
                     <Download className="w-4 h-4" /> Download JSON
                   </button>
                   <button onClick={() => setShowInstructions(false)} className="p-2.5 bg-slate-800 rounded-xl transition-all"><X className="w-5 h-5 text-slate-400 hover:text-white" /></button>
                </div>
              </div>
              <div className="flex-1 overflow-y-auto p-8 font-mono text-indigo-400 bg-slate-950/50 leading-relaxed text-lg scrollbar-thin scrollbar-thumb-slate-700">
                {instructionStrings.map((instruction, i) => {
                  const [motion, ...rest] = instruction.split(' ');
                  const valueText = rest.join(' ');
                  return (
                  <div key={i} className="flex gap-6 group hover:bg-indigo-500/5 p-1 rounded transition-colors">
                    <span className="text-slate-700 w-12 text-right select-none">{i + 1}</span>
                    <span className="flex items-center gap-4">
                      <span className="w-24 text-slate-400 font-bold">{motion}</span>
                      <span className="text-indigo-400">{valueText}</span>
                    </span>
                  </div>
                  );
                })}
                {instructionStrings.length === 0 && <div className="text-slate-600 text-center py-12 italic">No path instructions generated...</div>}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default App;
