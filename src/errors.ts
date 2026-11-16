// invariant: rejection messages start with one of these codes followed by ': '.
export const ErrorCodes = {
  INVALID_ARGUMENT: 'E_INVALID_ARGUMENT',
  DECODE_FAILED: 'E_DECODE_FAILED',
  SIZE_MISMATCH: 'E_SIZE_MISMATCH',
  ALIGNMENT_FAILED: 'E_ALIGNMENT_FAILED',
  WRITE_FAILED: 'E_WRITE_FAILED',
  INTERNAL: 'E_INTERNAL',
} as const;

export type ErrorCode = (typeof ErrorCodes)[keyof typeof ErrorCodes];
