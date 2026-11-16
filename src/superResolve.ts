import type { SuperResolveOptions, SuperResolveResult } from './types';

export function superResolve(
  _paths: string[],
  _options?: SuperResolveOptions
): Promise<SuperResolveResult> {
  return Promise.reject(
    new Error(
      "'react-native-mean-n-super-resolution' is only supported on native platforms."
    )
  );
}
