import type { SuperResolveResult } from 'react-native-mean-n-super-resolution';
import type { FrameCount } from './frameCounts';

export type CameraRun = {
  result: SuperResolveResult;
  // invariant: capture order; paths[0] is the reference frame.
  paths: string[];
  requested: FrameCount;
  // why: measures superResolve only, so capture speed does not skew the number.
  elapsedMs: number;
};

export type BurstEvent =
  | { type: 'progress'; captured: number; total: FrameCount }
  | { type: 'processing' };

export type BurstOutcome =
  | { ok: true; run: CameraRun }
  | { ok: false; stage: 'capture' | 'process'; message: string };

export type BurstDeps = {
  takeFrame: (index: number) => Promise<string>;
  deleteFiles: (paths: string[]) => Promise<void>;
  superResolve: (paths: string[]) => Promise<SuperResolveResult>;
  now: () => number;
};

export type BurstRunner = {
  start: (
    count: FrameCount,
    onEvent: (event: BurstEvent) => void
  ) => Promise<BurstOutcome> | null;
  isRunning: () => boolean;
};
