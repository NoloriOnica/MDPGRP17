
import { Point, Rect, DubinsEdge } from '../types';
import { euclidean, findValidDubinsPath } from './dubinsService';

const BACKTRACK_WINDOW = 3;
const BACKTRACK_PENALTY = 0.15;
const BEAM_WIDTH = 8;
const MAX_BRANCHING_PER_STATE = 6;
const REMAINING_HEURISTIC_WEIGHT = 0.35;
const UNREACHABLE_EDGE_PENALTY = 250;

interface SearchState {
  path: number[];
  visited: boolean[];
  visitedCount: number;
  travelCost: number;
  score: number;
}

interface CandidateMove {
  anchorPos: number;
  nextNode: number;
  backtrackLen: number;
  edgeLen: number;
  rank: number;
}

export const solveTSP = (
  goals: Point[],
  R: number,
  obstacles: Rect[]
): number[] => {
  const n = goals.length;
  if (n <= 1) return [0];

  const edgeCache: Map<string, DubinsEdge | null> = new Map();

  const getEdge = (i: number, j: number): DubinsEdge | null => {
    const key = `${i}-${j}`;
    if (edgeCache.has(key)) return edgeCache.get(key)!;
    const res = findValidDubinsPath(goals[i], goals[j], R, obstacles);
    edgeCache.set(key, res);
    return res;
  };

  const getBacktrackDistance = (path: number[], anchorPos: number, endPos: number): number => {
    let total = 0;
    for (let i = anchorPos + 1; i <= endPos; i++) {
      const edge = getEdge(path[i - 1], path[i]);
      if (!edge) return Infinity;
      total += edge.length;
    }
    return total;
  };

  const estimateRemainingCost = (currentNode: number, visited: boolean[]): number => {
    const remaining: number[] = [];
    for (let i = 1; i < n; i++) {
      if (!visited[i]) remaining.push(i);
    }
    if (remaining.length === 0) return 0;

    let total = 0;
    let cursor = currentNode;
    const pool = [...remaining];

    while (pool.length > 0) {
      let bestIdx = 0;
      let bestDist = Infinity;
      for (let i = 0; i < pool.length; i++) {
        const candidate = pool[i];
        const edge = getEdge(cursor, candidate);
        const distance = edge
          ? edge.length
          : euclidean(goals[cursor], goals[candidate]) + UNREACHABLE_EDGE_PENALTY;
        if (distance < bestDist) {
          bestDist = distance;
          bestIdx = i;
        }
      }
      total += bestDist;
      cursor = pool[bestIdx];
      pool.splice(bestIdx, 1);
    }

    return total * REMAINING_HEURISTIC_WEIGHT;
  };

  const buildVisitedKey = (visited: boolean[]): string => {
    return visited.map(flag => (flag ? '1' : '0')).join('');
  };

  const buildCandidatesForState = (state: SearchState): CandidateMove[] => {
    const moves: CandidateMove[] = [];
    const currentPos = state.path.length - 1;
    const anchorStart = Math.max(0, currentPos - BACKTRACK_WINDOW);

    for (let anchorPos = currentPos; anchorPos >= anchorStart; anchorPos--) {
      const anchorNode = state.path[anchorPos];
      const backtrackLen =
        anchorPos === currentPos ? 0 : getBacktrackDistance(state.path, anchorPos, currentPos);
      if (!Number.isFinite(backtrackLen)) continue;

      for (let candidate = 1; candidate < n; candidate++) {
        if (state.visited[candidate]) continue;
        const edge = getEdge(anchorNode, candidate);
        if (!edge) continue;

        const rank = edge.length + backtrackLen * (1 + BACKTRACK_PENALTY);
        moves.push({
          anchorPos,
          nextNode: candidate,
          backtrackLen,
          edgeLen: edge.length,
          rank
        });
      }
    }

    if (moves.length === 0) {
      const currentNode = state.path[currentPos];
      let fallbackNode: number | null = null;
      let fallbackDist = Infinity;
      for (let candidate = 1; candidate < n; candidate++) {
        if (state.visited[candidate]) continue;
        const dist = euclidean(goals[currentNode], goals[candidate]);
        if (dist < fallbackDist) {
          fallbackDist = dist;
          fallbackNode = candidate;
        }
      }
      if (fallbackNode !== null) {
        moves.push({
          anchorPos: currentPos,
          nextNode: fallbackNode,
          backtrackLen: 0,
          edgeLen: fallbackDist,
          rank: fallbackDist
        });
      }
    }

    moves.sort((a, b) => a.rank - b.rank || a.edgeLen - b.edgeLen);
    return moves.slice(0, MAX_BRANCHING_PER_STATE);
  };

  const makeNextState = (state: SearchState, move: CandidateMove): SearchState => {
    const currentPos = state.path.length - 1;
    const nextPath = [...state.path];

    if (move.anchorPos < currentPos) {
      for (let i = currentPos; i > move.anchorPos; i--) {
        nextPath.push(state.path[i - 1]);
      }
    }
    nextPath.push(move.nextNode);

    const nextVisited = [...state.visited];
    nextVisited[move.nextNode] = true;
    const nextTravel = state.travelCost + move.backtrackLen + move.edgeLen;
    const nextScore =
      nextTravel +
      move.backtrackLen * BACKTRACK_PENALTY +
      estimateRemainingCost(move.nextNode, nextVisited);

    return {
      path: nextPath,
      visited: nextVisited,
      visitedCount: state.visitedCount + 1,
      travelCost: nextTravel,
      score: nextScore
    };
  };

  const initialVisited = new Array(n).fill(false);
  initialVisited[0] = true;
  let frontier: SearchState[] = [
    {
      path: [0],
      visited: initialVisited,
      visitedCount: 1,
      travelCost: 0,
      score: estimateRemainingCost(0, initialVisited)
    }
  ];

  let bestComplete: SearchState | null = null;

  for (let depth = 1; depth < n; depth++) {
    const nextStates: SearchState[] = [];

    for (const state of frontier) {
      const candidates = buildCandidatesForState(state);
      for (const move of candidates) {
        const nextState = makeNextState(state, move);
        nextStates.push(nextState);
        if (nextState.visitedCount === n) {
          if (!bestComplete || nextState.travelCost < bestComplete.travelCost) {
            bestComplete = nextState;
          }
        }
      }
    }

    if (nextStates.length === 0) break;

    const deduped = new Map<string, SearchState>();
    for (const state of nextStates) {
      const lastNode = state.path[state.path.length - 1];
      const key = `${lastNode}:${buildVisitedKey(state.visited)}`;
      const previous = deduped.get(key);
      if (!previous) {
        deduped.set(key, state);
        continue;
      }
      if (state.score < previous.score || (state.score === previous.score && state.travelCost < previous.travelCost)) {
        deduped.set(key, state);
      }
    }

    frontier = [...deduped.values()]
      .sort((a, b) => a.score - b.score || a.travelCost - b.travelCost)
      .slice(0, BEAM_WIDTH);
  }

  if (bestComplete) return bestComplete.path;
  if (frontier.length > 0) {
    frontier.sort((a, b) => a.score - b.score || a.travelCost - b.travelCost);
    return frontier[0].path;
  }
  return [0];
};
