/**
 * `recolor.ts` — pure color-transform math for `.edds` recoloring.
 *
 * No FFI, no Workbench, no filesystem. Every function operates on raw RGBA
 * pixels (Uint8 0–255) so it can be unit-tested directly and reused by the
 * MCP tool. The pipeline applies a list of operations to each pixel:
 *
 *   - `hue_shift`: rotate hue by N degrees (HSL space)
 *   - `saturation`: scale saturation (0 = grayscale, 1 = unchanged)
 *   - `lightness`: scale lightness/brightness (1 = unchanged)
 *   - `tint`: mix toward a target RGB color by strength (0–1)
 *   - `whiten`: desaturate to luminance then lift toward white (0–1 strength;
 *     1 = pure white, preserves shading at lower strengths)
 *
 * Ops are applied in order, each after the last, so `[{saturation:0},
 * {whiten:0.85}]` first desaturates then whitens.
 */

// ── Operation types ──────────────────────────────────────────────────────────

export interface HueShiftOp {
  hue_shift: number;
}
export interface SaturationOp {
  saturation: number;
}
export interface LightnessOp {
  lightness: number;
}
export interface TintOp {
  tint: string;
  tint_strength: number;
}
export interface WhitenOp {
  whiten: number;
}

export type RecolorOp = HueShiftOp | SaturationOp | LightnessOp | TintOp | WhitenOp;

// ── RGB ↔ HSL ────────────────────────────────────────────────────────────────

/**
 * Convert an RGB triple (0–255) to HSL. h in [0,360), s/l in [0,1].
 */
export function rgbToHsl(r: number, g: number, b: number): [number, number, number] {
  const rn = r / 255;
  const gn = g / 255;
  const bn = b / 255;
  const max = Math.max(rn, gn, bn);
  const min = Math.min(rn, gn, bn);
  const l = (max + min) / 2;
  if (max === min) return [0, 0, l];
  const d = max - min;
  const s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
  let h: number;
  switch (max) {
    case rn:
      h = ((gn - bn) / d + (gn < bn ? 6 : 0)) * 60;
      break;
    case gn:
      h = ((bn - rn) / d + 2) * 60;
      break;
    default:
      h = ((rn - gn) / d + 4) * 60;
  }
  return [h, s, l];
}

/**
 * Convert HSL back to RGB (0–255). h in [0,360), s/l in [0,1].
 */
export function hslToRgb(h: number, s: number, l: number): [number, number, number] {
  const hn = ((h % 360) + 360) % 360 / 360;
  if (s === 0) {
    const v = Math.round(l * 255);
    return [v, v, v];
  }
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const p = 2 * l - q;
  const hue2rgb = (t0: number): number => {
    let t = t0;
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1 / 6) return p + (q - p) * 6 * t;
    if (t < 1 / 2) return q;
    if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
    return p;
  };
  const r = Math.round(hue2rgb(hn + 1 / 3) * 255);
  const g = Math.round(hue2rgb(hn) * 255);
  const b = Math.round(hue2rgb(hn - 1 / 3) * 255);
  return [r, g, b];
}

// ── Hex color parsing ─────────────────────────────────────────────────────────

const HEX_RE = /^#?([0-9a-fA-F]{6})$/;

/** Parse "#RRGGBB" (or "RRGGBB") to [r,g,b] 0–255. Throws on malformed input. */
export function parseHexColor(hex: string): [number, number, number] {
  const m = HEX_RE.exec(hex.trim());
  if (!m) throw new Error(`Invalid hex color "${hex}" — expected #RRGGBB`);
  const v = parseInt(m[1], 16);
  return [(v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff];
}

/** Luminance of an RGB triple (Rec. 709), 0–255. */
export function luminance(r: number, g: number, b: number): number {
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

// ── Single-pixel transform ───────────────────────────────────────────────────

/**
 * Apply the full op list to one RGB triple. Returns new [r,g,b] 0–255.
 * Ops are applied in array order.
 */
export function applyOps(rgb: [number, number, number], ops: RecolorOp[]): [number, number, number] {
  let [r, g, b] = rgb;
  for (const op of ops) {
    if ("hue_shift" in op && op.hue_shift !== 0) {
      const [h, s, l] = rgbToHsl(r, g, b);
      [r, g, b] = hslToRgb(h + op.hue_shift, s, l);
    } else if ("saturation" in op && op.saturation !== 1) {
      const [h, s, l] = rgbToHsl(r, g, b);
      [r, g, b] = hslToRgb(h, Math.min(1, Math.max(0, s * op.saturation)), l);
    } else if ("lightness" in op && op.lightness !== 1) {
      const [h, s, l] = rgbToHsl(r, g, b);
      [r, g, b] = hslToRgb(h, s, Math.min(1, Math.max(0, l * op.lightness)));
    } else if ("tint" in op) {
      const [tr, tg, tb] = parseHexColor(op.tint);
      const st = Math.min(1, Math.max(0, op.tint_strength));
      r = r + (tr - r) * st;
      g = g + (tg - g) * st;
      b = b + (tb - b) * st;
    } else if ("whiten" in op && op.whiten > 0) {
      const lum = luminance(r, g, b);
      const st = Math.min(1, Math.max(0, op.whiten));
      r = lum + (255 - lum) * st;
      g = lum + (255 - lum) * st;
      b = lum + (255 - lum) * st;
    }
  }
  return [
    Math.round(Math.min(255, Math.max(0, r))),
    Math.round(Math.min(255, Math.max(0, g))),
    Math.round(Math.min(255, Math.max(0, b))),
  ];
}

/**
 * Transform a full RGBA buffer (4 bytes/px, 0–255) in place, preserving alpha.
 * @returns the same buffer (mutated)
 */
export function transformRgbaInPlace(rgba: Buffer, ops: RecolorOp[]): Buffer {
  for (let i = 0; i + 3 < rgba.length; i += 4) {
    const [r, g, b] = applyOps([rgba[i], rgba[i + 1], rgba[i + 2]], ops);
    rgba[i] = r;
    rgba[i + 1] = g;
    rgba[i + 2] = b;
  }
  return rgba;
}