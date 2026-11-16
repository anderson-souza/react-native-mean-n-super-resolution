import { ErrorCodes } from './errors';

const FRAME_COUNTS = [5, 15, 30];
const SCALES = [1, 2, 3, 4];
const EXTENSIONS = ['.jpg', '.jpeg', '.png'];

function invalid(detail: string): string {
  return `${ErrorCodes.INVALID_ARGUMENT}: ${detail}`;
}

function lowerExt(path: string): string {
  const base = path.slice(path.lastIndexOf('/') + 1);
  const dot = base.lastIndexOf('.');
  return dot < 0 ? '' : base.slice(dot).toLowerCase();
}

// invariant: mirrors cpp/sr/Validate.cpp; returns null iff the native call may start.
export function validateArgs(paths: unknown, options: unknown): string | null {
  if (!Array.isArray(paths)) return invalid('paths must be an array');
  if (!FRAME_COUNTS.includes(paths.length)) {
    return invalid(
      `paths must contain exactly 5, 15 or 30 entries, got ${paths.length}`
    );
  }
  const bad = paths.findIndex((p) => typeof p !== 'string' || p === '');
  if (bad >= 0) return invalid(`paths[${bad}] must be a non-empty string`);

  if (options === undefined) return null;
  if (typeof options !== 'object' || options === null) {
    return invalid('options must be an object');
  }
  const { scale, outputPath } = options as Record<string, unknown>;
  if (scale !== undefined && !SCALES.includes(scale as number)) {
    return invalid(`scale must be 1, 2, 3 or 4, got ${String(scale)}`);
  }
  if (
    outputPath !== undefined &&
    (typeof outputPath !== 'string' ||
      !EXTENSIONS.includes(lowerExt(outputPath)))
  ) {
    return invalid(
      `outputPath must end in .jpg, .jpeg or .png: '${String(outputPath)}'`
    );
  }
  return null;
}
