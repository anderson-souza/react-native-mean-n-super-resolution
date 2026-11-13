// why: the example needs 5 same-scene frames with known sub-frame shifts, and a
// procedurally generated scene keeps the repo free of third-party photos.
// Usage: node example/scripts/generate-frames.mjs  (rewrites example/assets/frames/*.png)
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { deflateSync } from 'node:zlib';

const WIDTH = 640;
const HEIGHT = 480;
const MARGIN = 16;
// invariant: frame 0 is the reference crop; the others are integer shifts of it
// (<= 5 % of the frame size, the range SR-CORE-04 promises to align).
const OFFSETS = [
  [0, 0],
  [5, 2],
  [-3, 6],
  [8, -4],
  [-6, -7],
];

function mulberry32(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function renderScene(w, h) {
  const rand = mulberry32(42);
  const blobs = Array.from({ length: 60 }, () => ({
    x: rand() * w,
    y: rand() * h,
    r: 8 + rand() * 60,
    c: [rand() * 255, rand() * 255, rand() * 255],
  }));
  const rgb = new Uint8Array(w * h * 3);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let r = 128 + 60 * Math.sin(x / 23) * Math.cos(y / 31);
      let g = 128 + 60 * Math.sin((x + y) / 41);
      let b = 128 + 60 * Math.cos(x / 17 - y / 29);
      for (const blob of blobs) {
        const d2 = (x - blob.x) ** 2 + (y - blob.y) ** 2;
        if (d2 < blob.r * blob.r) {
          r = (r + blob.c[0]) / 2;
          g = (g + blob.c[1]) / 2;
          b = (b + blob.c[2]) / 2;
        }
      }
      const checker = ((x >> 3) + (y >> 3)) & 1 ? 12 : -12;
      const i = (y * w + x) * 3;
      rgb[i] = Math.max(0, Math.min(255, Math.round(r + checker)));
      rgb[i + 1] = Math.max(0, Math.min(255, Math.round(g + checker)));
      rgb[i + 2] = Math.max(0, Math.min(255, Math.round(b + checker)));
    }
  }
  return rgb;
}

const CRC_TABLE = Array.from({ length: 256 }, (_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});

function crc32(buf) {
  let c = 0xffffffff;
  for (const byte of buf) c = CRC_TABLE[(c ^ byte) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function chunk(type, data) {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

function encodePng(rgb, w, h) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 2; // color type RGB
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (w * 3 + 1)] = 0; // filter: none
    Buffer.from(rgb.buffer, y * w * 3, w * 3).copy(raw, y * (w * 3 + 1) + 1);
  }
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr),
    chunk('IDAT', deflateSync(raw, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

const sceneW = WIDTH + 2 * MARGIN;
const sceneH = HEIGHT + 2 * MARGIN;
const scene = renderScene(sceneW, sceneH);
const outDir = join(dirname(fileURLToPath(import.meta.url)), '..', 'assets', 'frames');
mkdirSync(outDir, { recursive: true });

OFFSETS.forEach(([dx, dy], index) => {
  const crop = new Uint8Array(WIDTH * HEIGHT * 3);
  for (let y = 0; y < HEIGHT; y++) {
    const src = ((y + MARGIN + dy) * sceneW + MARGIN + dx) * 3;
    crop.set(scene.subarray(src, src + WIDTH * 3), y * WIDTH * 3);
  }
  const file = join(outDir, `frame${index}.png`);
  writeFileSync(file, encodePng(crop, WIDTH, HEIGHT));
  console.log(`${file} (${WIDTH}x${HEIGHT}, shift ${dx},${dy})`);
});
