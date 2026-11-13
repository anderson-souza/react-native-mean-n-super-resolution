import type { FrameCount } from './frameCounts';
import type { BurstDeps, BurstEvent, BurstOutcome, BurstRunner } from './types';

function messageOf(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

export function createBurstRunner(deps: BurstDeps): BurstRunner {
  let running = false;
  let previousPaths: string[] = [];

  const run = async (
    count: FrameCount,
    onEvent: (event: BurstEvent) => void
  ): Promise<BurstOutcome> => {
    const paths: string[] = [];
    try {
      try {
        await deps.deleteFiles(previousPaths);
      } catch (e) {
        // why: stale cache files must never block a new burst (CAM-14).
        console.log(`[camera-burst] cleanup failed: ${messageOf(e)}`);
      }
      previousPaths = [];

      for (let i = 0; i < count; i++) {
        try {
          paths.push(await deps.takeFrame(i));
        } catch (e) {
          return { ok: false, stage: 'capture', message: messageOf(e) };
        }
        onEvent({ type: 'progress', captured: i + 1, total: count });
      }

      onEvent({ type: 'processing' });
      const start = deps.now();
      try {
        const result = await deps.superResolve(paths);
        return {
          ok: true,
          run: {
            result,
            paths: [...paths],
            requested: count,
            elapsedMs: deps.now() - start,
          },
        };
      } catch (e) {
        return { ok: false, stage: 'process', message: messageOf(e) };
      }
    } finally {
      previousPaths = paths;
      running = false;
    }
  };

  return {
    start(count, onEvent) {
      // invariant: flag set synchronously so a second tap in the same frame is rejected (CAM-13).
      if (running) return null;
      running = true;
      return run(count, onEvent);
    },
    isRunning: () => running,
  };
}
