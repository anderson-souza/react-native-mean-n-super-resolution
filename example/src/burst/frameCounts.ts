// invariant: mirrors the frame counts superResolve accepts (README "Frame count").
export const FRAME_COUNTS = [5, 15, 30] as const;

export type FrameCount = (typeof FRAME_COUNTS)[number];

export const DEFAULT_FRAME_COUNT: FrameCount = 5;
