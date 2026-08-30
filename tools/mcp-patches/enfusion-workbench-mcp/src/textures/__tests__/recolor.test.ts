import { describe, it, expect } from "vitest";
import {
  rgbToHsl,
  hslToRgb,
  parseHexColor,
  luminance,
  applyOps,
  transformRgbaInPlace,
} from "../recolor.js";

describe("recolor — RGB↔HSL", () => {
  it("round-trips through HSL", () => {
    const samples: Array<[number, number, number]> = [
      [255, 0, 0],
      [0, 255, 0],
      [0, 0, 255],
      [128, 128, 128],
      [231, 123, 0], // sandbag orange
      [66, 123, 222], // sandbag blue-ish
      [255, 255, 255],
      [0, 0, 0],
    ];
    for (const [r, g, b] of samples) {
      const [h, s, l] = rgbToHsl(r, g, b);
      const [rr, gg, bb] = hslToRgb(h, s, l);
      expect(Math.abs(rr - r)).toBeLessThanOrEqual(1);
      expect(Math.abs(gg - g)).toBeLessThanOrEqual(1);
      expect(Math.abs(bb - b)).toBeLessThanOrEqual(1);
    }
  });

  it("grayscale has zero saturation", () => {
    const [h, s] = rgbToHsl(128, 128, 128);
    expect(s).toBe(0);
    expect(h).toBe(0);
  });

  it("pure red is hue 0", () => {
    const [h] = rgbToHsl(255, 0, 0);
    expect(h).toBe(0);
  });
});

describe("recolor — hex parsing", () => {
  it("parses #RRGGBB and bare RRGGBB", () => {
    expect(parseHexColor("#FFFFFF")).toEqual([255, 255, 255]);
    expect(parseHexColor("ff0000")).toEqual([255, 0, 0]);
    expect(parseHexColor("#00ff00")).toEqual([0, 255, 0]);
  });

  it("throws on malformed input", () => {
    expect(() => parseHexColor("white")).toThrow();
    expect(() => parseHexColor("#FFF")).toThrow();
    expect(() => parseHexColor("")).toThrow();
  });
});

describe("recolor — luminance", () => {
  it("is 255 for white and 0 for black", () => {
    expect(luminance(255, 255, 255)).toBeCloseTo(255, 5);
    expect(luminance(0, 0, 0)).toBeCloseTo(0, 5);
  });
});

describe("recolor — ops", () => {
  it("whiten pushes a dark color toward white", () => {
    const [r, g, b] = applyOps([100, 60, 30], [{ whiten: 0.9 }]);
    expect(r).toBeGreaterThan(200);
    expect(g).toBeGreaterThan(200);
    expect(b).toBeGreaterThan(200);
  });

  it("whiten at strength 1 is pure white", () => {
    const [r, g, b] = applyOps([30, 40, 50], [{ whiten: 1 }]);
    expect([r, g, b]).toEqual([255, 255, 255]);
  });

  it("saturation 0 makes grayscale (r=g=b)", () => {
    const [r, g, b] = applyOps([231, 123, 0], [{ saturation: 0 }]);
    expect(r).toBe(g);
    expect(g).toBe(b);
  });

  it("hue shift moves a known color", () => {
    // Red + 120° = green
    const [r, g] = applyOps([255, 0, 0], [{ hue_shift: 120 }]);
    expect(g).toBeGreaterThan(200);
    expect(r).toBeLessThan(100);
  });

  it("tint blends toward target color", () => {
    const [r, g, b] = applyOps([0, 0, 0], [{ tint: "#FFFFFF", tint_strength: 0.5 }]);
    expect(r).toBe(128);
    expect(g).toBe(128);
    expect(b).toBe(128);
  });

  it("applies ops in order", () => {
    // Tint to red first (fully red), then saturation 0 → gray, then whiten
    const [r, g, b] = applyOps([100, 100, 100], [
      { tint: "#FF0000", tint_strength: 1 },
      { saturation: 0 },
      { whiten: 0.5 },
    ]);
    // After saturation 0 the pixel is gray (r=g=b); whiten keeps it gray
    expect(r).toBe(g);
    expect(g).toBe(b);
    // whiten 0.5 lifts 128 → 192 (between gray and white)
    expect(r).toBe(192);
  });
});

describe("recolor — in-place RGBA buffer", () => {
  it("transforms pixels and preserves alpha", () => {
    // 2 pixels RGBA
    const buf = Buffer.from([100, 60, 30, 255, 10, 20, 30, 128]);
    transformRgbaInPlace(buf, [{ whiten: 1 }]);
    expect(buf[0]).toBe(255);
    expect(buf[1]).toBe(255);
    expect(buf[2]).toBe(255);
    expect(buf[3]).toBe(255); // alpha preserved
    expect(buf[7]).toBe(128); // alpha preserved on second pixel
  });
});