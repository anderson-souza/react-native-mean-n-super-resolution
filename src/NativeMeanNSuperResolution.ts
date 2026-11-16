import { TurboModuleRegistry, type TurboModule } from 'react-native';

// invariant: codegen reads these aliases from this file only; they generate the
// C++ structs used by cpp/MeanNSuperResolutionImpl. Public types live in types.ts.
export type SuperResolveOptions = {
  scale?: number;
  outputPath?: string;
};

export type SuperResolveResult = {
  path: string;
  width: number;
  height: number;
  framesUsed: number;
};

export interface Spec extends TurboModule {
  superResolve(
    paths: string[],
    options: SuperResolveOptions
  ): Promise<SuperResolveResult>;
}

export default TurboModuleRegistry.getEnforcing<Spec>('MeanNSuperResolution');
