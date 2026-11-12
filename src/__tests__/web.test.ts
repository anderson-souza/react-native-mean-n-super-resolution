import { expect, it, jest } from '@jest/globals';

// why: the RN jest preset resolves '../superResolve' to the .native file; the
// explicit extension loads the web fallback instead.
const { superResolve } =
  jest.requireActual<typeof import('../superResolve')>('../superResolve.ts');

it('rejects on web with the native-only message (SR-ERR-09)', async () => {
  await expect(
    superResolve(['/a/0.jpg', '/a/1.jpg', '/a/2.jpg', '/a/3.jpg', '/a/4.jpg'])
  ).rejects.toThrow(
    "'react-native-mean-n-super-resolution' is only supported on native platforms."
  );
});
