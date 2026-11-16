import { describe, expect, it, jest } from '@jest/globals';
import type { SuperResolveResult } from 'react-native-mean-n-super-resolution';
import { createBurstRunner } from '../burstRunner';
import { DEFAULT_FRAME_COUNT, FRAME_COUNTS } from '../frameCounts';
import type { BurstDeps, BurstEvent } from '../types';

const srResult: SuperResolveResult = {
  path: 'file:///cache/superres-1.jpg',
  width: 2160,
  height: 2880,
  framesUsed: 4,
};

type Call = { kind: 'delete'; paths: string[] } | { kind: 'take'; i: number };

function setup(overrides: Partial<BurstDeps> = {}) {
  const calls: Call[] = [];
  let burst = 0;
  let clock = 1000;
  // why: capture and cleanup also advance the clock, so elapsedMs only equals
  // 250 when timing wraps superResolve alone (CAM-06).
  const takeFrame = jest.fn(async (i: number) => {
    calls.push({ kind: 'take', i });
    clock += 100;
    return `/cache/burst-${burst}-${i}.jpg`;
  });
  const deleteFiles = jest.fn(async (paths: string[]) => {
    calls.push({ kind: 'delete', paths: [...paths] });
    clock += 10;
  });
  const superResolve = jest.fn(async (_paths: string[]) => {
    clock += 250;
    return srResult;
  });
  const deps: BurstDeps = {
    takeFrame,
    deleteFiles,
    superResolve,
    now: () => clock,
    ...overrides,
  };
  const runner = createBurstRunner(deps);
  const nextBurst = () => {
    burst += 1;
  };
  return {
    runner,
    deps,
    calls,
    takeFrame,
    deleteFiles,
    superResolve,
    nextBurst,
  };
}

const pathsFor = (burst: number, n: number) =>
  Array.from({ length: n }, (_, i) => `/cache/burst-${burst}-${i}.jpg`);

describe('frameCounts (CAM-03)', () => {
  it('offers exactly 5, 15 and 30 with 5 as default', () => {
    expect([...FRAME_COUNTS]).toEqual([5, 15, 30]);
    expect(DEFAULT_FRAME_COUNT).toBe(5);
  });
});

describe('burstRunner capture + process', () => {
  it.each([5, 15, 30] as const)(
    'captures %i frames in order with i/N progress, then processes (CAM-04, CAM-05)',
    async (n) => {
      const { runner, takeFrame, superResolve } = setup();
      const events: BurstEvent[] = [];
      const outcome = await runner.start(n, (e) => events.push(e));

      expect(takeFrame.mock.calls.map((c) => c[0])).toEqual(
        Array.from({ length: n }, (_, i) => i)
      );
      expect(events).toEqual([
        ...Array.from({ length: n }, (_, i) => ({
          type: 'progress',
          captured: i + 1,
          total: n,
        })),
        { type: 'processing' },
      ]);
      expect(superResolve).toHaveBeenCalledTimes(1);
      expect(superResolve.mock.calls[0]?.[0]).toEqual(pathsFor(0, n));
      expect(outcome?.ok).toBe(true);
    }
  );

  it('reports i/N only after frame i has been captured (CAM-04)', async () => {
    const log: string[] = [];
    const runner = createBurstRunner({
      takeFrame: async (i) => {
        await Promise.resolve();
        log.push(`frame${i}`);
        return `/cache/f${i}.jpg`;
      },
      deleteFiles: async () => {},
      superResolve: async () => srResult,
      now: () => 0,
    });
    await runner.start(5, (e) =>
      log.push(e.type === 'progress' ? `progress${e.captured}` : e.type)
    );
    expect(log).toEqual([
      'frame0',
      'progress1',
      'frame1',
      'progress2',
      'frame2',
      'progress3',
      'frame3',
      'progress4',
      'frame4',
      'progress5',
      'processing',
    ]);
  });

  it('emits no progress for a frame that failed (CAM-04, CAM-11)', async () => {
    const events: BurstEvent[] = [];
    const runner = createBurstRunner({
      takeFrame: async (i) => {
        if (i === 2) throw new Error('snapshot failed');
        return `/cache/f${i}.jpg`;
      },
      deleteFiles: async () => {},
      superResolve: async () => srResult,
      now: () => 0,
    });
    await runner.start(5, (e) => events.push(e));
    expect(events).toEqual([
      { type: 'progress', captured: 1, total: 5 },
      { type: 'progress', captured: 2, total: 5 },
    ]);
  });

  it('calls superResolve only after the processing event', async () => {
    const order: string[] = [];
    const { runner } = setup({
      superResolve: async () => {
        order.push('superResolve');
        return srResult;
      },
    });
    await runner.start(5, (e) => order.push(e.type));
    expect(order.slice(-2)).toEqual(['processing', 'superResolve']);
  });

  it('returns result, capture-order paths, requested N and superResolve-only elapsed time (CAM-06)', async () => {
    const { runner } = setup();
    const outcome = await runner.start(15, () => {});
    expect(outcome).toEqual({
      ok: true,
      run: {
        result: srResult,
        paths: pathsFor(0, 15),
        requested: 15,
        elapsedMs: 250,
      },
    });
  });

  it('passes the superResolve rejection message through unchanged (CAM-10)', async () => {
    const { runner } = setup({
      superResolve: async () => {
        throw new Error('E_ALIGNMENT_FAILED: only 1 of 5 frames aligned');
      },
    });
    const outcome = await runner.start(5, () => {});
    expect(outcome).toEqual({
      ok: false,
      stage: 'process',
      message: 'E_ALIGNMENT_FAILED: only 1 of 5 frames aligned',
    });
  });

  it('stops at a failed frame and never calls superResolve (CAM-11)', async () => {
    const { superResolve } = setup();
    const takeFrame = jest.fn(async (i: number) => {
      if (i === 3) throw new Error('Cannot take snapshot');
      return `/cache/f${i}.jpg`;
    });
    const failing = createBurstRunner({
      takeFrame,
      deleteFiles: async () => {},
      superResolve,
      now: () => 0,
    });
    const events: BurstEvent[] = [];
    const outcome = await failing.start(15, (e) => events.push(e));

    expect(outcome).toEqual({
      ok: false,
      stage: 'capture',
      message: 'Cannot take snapshot',
    });
    expect(takeFrame).toHaveBeenCalledTimes(4);
    expect(superResolve).not.toHaveBeenCalled();
    expect(events.map((e) => e.type)).not.toContain('processing');
    expect(failing.isRunning()).toBe(false);
  });
});

describe('burstRunner single-run guard (CAM-13)', () => {
  it('rejects a second start while running and captures only N frames', async () => {
    const { runner, takeFrame } = setup();
    const first = runner.start(5, () => {});
    expect(runner.isRunning()).toBe(true);
    const second = runner.start(5, () => {});
    expect(second).toBeNull();
    await first;
    expect(takeFrame).toHaveBeenCalledTimes(5);
    expect(runner.isRunning()).toBe(false);
  });

  it('is idle again after a failed burst', async () => {
    const { runner } = setup({
      superResolve: async () => {
        throw new Error('E_SIZE_MISMATCH: index=2');
      },
    });
    await runner.start(5, () => {});
    expect(runner.isRunning()).toBe(false);
    const again = runner.start(5, () => {});
    expect(again).not.toBeNull();
    await again;
  });
});

describe('burstRunner cleanup (CAM-14)', () => {
  it('deletes nothing before the first burst', async () => {
    const { runner, deleteFiles } = setup();
    await runner.start(5, () => {});
    expect(deleteFiles).toHaveBeenCalledTimes(1);
    expect(deleteFiles.mock.calls[0]?.[0]).toEqual([]);
  });

  it("deletes the previous burst's files before the next burst's first frame", async () => {
    const { runner, calls, nextBurst } = setup();
    await runner.start(5, () => {});
    nextBurst();
    calls.length = 0;
    await runner.start(5, () => {});
    expect(calls[0]).toEqual({ kind: 'delete', paths: pathsFor(0, 5) });
    expect(calls[1]).toEqual({ kind: 'take', i: 0 });
  });

  it('deletes partial files left by a failed capture on the next burst', async () => {
    let fail = true;
    const deleted: string[][] = [];
    const runner = createBurstRunner({
      takeFrame: async (i) => {
        if (fail && i === 2) throw new Error('snapshot failed');
        return `/cache/p${i}.jpg`;
      },
      deleteFiles: async (paths) => {
        deleted.push([...paths]);
      },
      superResolve: async () => srResult,
      now: () => 0,
    });
    await runner.start(5, () => {});
    fail = false;
    await runner.start(5, () => {});
    expect(deleted[1]).toEqual(['/cache/p0.jpg', '/cache/p1.jpg']);
  });

  it('keeps going when deleting old files fails', async () => {
    const log = jest.spyOn(console, 'log').mockImplementation(() => {});
    const { runner, takeFrame } = setup({
      deleteFiles: async () => {
        throw new Error('EACCES');
      },
    });
    const outcome = await runner.start(5, () => {});
    expect(takeFrame).toHaveBeenCalledTimes(5);
    expect(outcome?.ok).toBe(true);
    log.mockRestore();
  });
});

describe('burstRunner frame count change (edge case)', () => {
  it('uses the newly selected N on the next burst', async () => {
    const { runner, takeFrame, superResolve } = setup();
    await runner.start(5, () => {});
    await runner.start(30, () => {});
    expect(takeFrame).toHaveBeenCalledTimes(35);
    expect(superResolve.mock.calls[1]?.[0]).toHaveLength(30);
  });
});
