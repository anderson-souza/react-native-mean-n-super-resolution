import { beforeEach, describe, expect, it, jest } from '@jest/globals';

const mockNativeSuperResolve =
  jest.fn<(...args: unknown[]) => Promise<unknown>>();

jest.mock('../NativeMeanNSuperResolution', () => ({
  __esModule: true,
  default: {
    superResolve: (...args: unknown[]) => mockNativeSuperResolve(...args),
  },
}));

// invariant: resolves to superResolve.native.ts under the RN jest preset.
const { superResolve } =
  require('../superResolve') as typeof import('../superResolve.native');

const fivePaths = ['/a/0.jpg', '/a/1.jpg', '/a/2.jpg', '/a/3.jpg', '/a/4.jpg'];
const pathsOf = (n: number) =>
  Array.from({ length: n }, (_, i) => `/a/${i}.jpg`);

const nativeResult = {
  path: 'file:///tmp/superres-1.jpg',
  width: 200,
  height: 100,
  framesUsed: 5,
};

beforeEach(() => {
  mockNativeSuperResolve.mockReset();
  mockNativeSuperResolve.mockResolvedValue(nativeResult);
});

async function expectInvalidArgument(call: () => Promise<unknown>) {
  await expect(call()).rejects.toThrow(/^E_INVALID_ARGUMENT/);
  expect(mockNativeSuperResolve).not.toHaveBeenCalled();
}

describe('superResolve argument validation (SR-ERR-01)', () => {
  it.each([0, 1, 4, 6, 14, 16, 29, 31])(
    'rejects %i paths without calling native',
    async (n) => {
      await expectInvalidArgument(() => superResolve(pathsOf(n)));
    }
  );

  it.each([5, 15, 30])('accepts %i paths', async (n) => {
    await expect(superResolve(pathsOf(n))).resolves.toEqual(nativeResult);
    expect(mockNativeSuperResolve).toHaveBeenCalledTimes(1);
  });

  it.each([
    ['a string', '/a/0.jpg'],
    ['null', null],
    ['undefined', undefined],
    ['an object', { length: 5 }],
  ])('rejects paths that is %s', async (_label, paths) => {
    await expectInvalidArgument(() => superResolve(paths as never));
  });

  it('rejects a non-string entry', async () => {
    const paths: unknown[] = [...fivePaths];
    paths[2] = 42;
    await expectInvalidArgument(() => superResolve(paths as never));
  });

  it('rejects an empty-string entry', async () => {
    const paths = [...fivePaths];
    paths[4] = '';
    await expectInvalidArgument(() => superResolve(paths));
  });
});

describe('superResolve scale validation (SR-ERR-02)', () => {
  it.each([0, 5, 2.5, '2', -1])('rejects scale %p', async (scale) => {
    await expectInvalidArgument(() =>
      superResolve(fivePaths, { scale: scale as never })
    );
  });

  it.each([1, 2, 3, 4] as const)('accepts scale %i', async (scale) => {
    await superResolve(fivePaths, { scale });
    expect(mockNativeSuperResolve).toHaveBeenCalledWith(fivePaths, { scale });
  });
});

describe('superResolve outputPath validation (SR-ERR-03)', () => {
  it.each(['/out/x.bmp', '/out/x', '/out/x.jpg.txt', '', '/out.d/x'])(
    'rejects outputPath %p',
    async (outputPath) => {
      await expectInvalidArgument(() =>
        superResolve(fivePaths, { outputPath })
      );
    }
  );

  it('rejects a non-string outputPath', async () => {
    await expectInvalidArgument(() =>
      superResolve(fivePaths, { outputPath: 7 as never })
    );
  });

  it.each([
    '/out/x.jpg',
    '/out/x.JPG',
    '/out/x.jpeg',
    '/out/x.Png',
    'file:///out/my%20x.png',
  ])('accepts outputPath %p', async (outputPath) => {
    await superResolve(fivePaths, { outputPath });
    expect(mockNativeSuperResolve).toHaveBeenCalledWith(fivePaths, {
      outputPath,
    });
  });
});

describe('superResolve native pass-through (SR-CORE-01)', () => {
  it('passes an empty options object when none is given', async () => {
    await superResolve(fivePaths);
    expect(mockNativeSuperResolve).toHaveBeenCalledWith(fivePaths, {});
  });

  it('passes options through unchanged', async () => {
    const options = { scale: 3 as const, outputPath: '/out/r.png' };
    await superResolve(fivePaths, options);
    expect(mockNativeSuperResolve).toHaveBeenCalledWith(fivePaths, options);
  });

  it('resolves with the native result unchanged', async () => {
    await expect(superResolve(fivePaths)).resolves.toBe(nativeResult);
  });

  it('propagates a native rejection unchanged', async () => {
    const nativeError = new Error('E_DECODE_FAILED: index=2 could not decode');
    mockNativeSuperResolve.mockRejectedValue(nativeError);
    await expect(superResolve(fivePaths)).rejects.toBe(nativeError);
  });
});
