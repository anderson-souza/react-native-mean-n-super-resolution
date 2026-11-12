import { expect, it, jest } from '@jest/globals';

jest.mock('../NativeMeanNSuperResolution', () => ({
  __esModule: true,
  default: { superResolve: jest.fn() },
}));

const api = require('../index') as Record<string, unknown>;

it('exports superResolve and ErrorCodes, and no multiply (SR-DOC-01)', () => {
  expect(typeof api.superResolve).toBe('function');
  expect(api.ErrorCodes).toEqual({
    INVALID_ARGUMENT: 'E_INVALID_ARGUMENT',
    DECODE_FAILED: 'E_DECODE_FAILED',
    SIZE_MISMATCH: 'E_SIZE_MISMATCH',
    ALIGNMENT_FAILED: 'E_ALIGNMENT_FAILED',
    WRITE_FAILED: 'E_WRITE_FAILED',
    INTERNAL: 'E_INTERNAL',
  });
  expect('multiply' in api).toBe(false);
});
