import MeanNSuperResolution from './NativeMeanNSuperResolution';
import type { SuperResolveOptions, SuperResolveResult } from './types';
import { validateArgs } from './validate';

export function superResolve(
  paths: string[],
  options?: SuperResolveOptions
): Promise<SuperResolveResult> {
  const error = validateArgs(paths, options);
  if (error !== null) return Promise.reject(new Error(error));
  return MeanNSuperResolution.superResolve(paths, options ?? {});
}
