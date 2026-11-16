export type SuperResolveOptions = {
  /** Integer upscale factor. Default `2`. */
  scale?: 1 | 2 | 3 | 4;
  /** Absolute path or `file://` URI ending in `.jpg`, `.jpeg` or `.png`. */
  outputPath?: string;
};

export type SuperResolveResult = {
  /** `file://` URI of the written image. */
  path: string;
  width: number;
  height: number;
  /** Frames averaged, reference included (2..N). */
  framesUsed: number;
};
