// Proof diagram: a 3-step flow with arrows in the paper-notice palette.
// The paper fill is opaque: transparency (alpha through TextureUI.conf) is not proven in game yet.
// Paper (0.890 0.851 0.761) = #E3D9C2, ink (0.110 0.102 0.086) = #1C1A16.
export const PAPER = "#E3D9C2";
export const INK = "#1C1A16";
export function flowSvg() {
  const box = (x, label, n) => `
  <rect x="${x}" y="88" width="56" height="80" fill="none" stroke="${INK}" stroke-width="4"/>
  <text x="${x + 28}" y="136" font-family="Arial, Helvetica, sans-serif" font-weight="700" font-size="40" text-anchor="middle" fill="${INK}">${n}</text>
  <text x="${x + 28}" y="196" font-family="Arial, Helvetica, sans-serif" font-weight="700" font-size="20" text-anchor="middle" fill="${INK}">${label}</text>`;
  const arrow = (x) => `
  <line x1="${x}" y1="128" x2="${x + 16}" y2="128" stroke="${INK}" stroke-width="4"/>
  <polygon points="${x + 26},128 ${x + 14},120 ${x + 14},136" fill="${INK}"/>`;
  return `<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <rect width="256" height="256" fill="${PAPER}"/>
  <text x="128" y="52" font-family="Arial, Helvetica, sans-serif" font-weight="700" font-size="26" text-anchor="middle" fill="${INK}">MEDIC CHAIN</text>
  <line x1="24" y1="66" x2="232" y2="66" stroke="${INK}" stroke-width="3"/>${box(14, "CALL", 1)}${arrow(72)}${box(100, "TREAT", 2)}${arrow(158)}${box(186, "BED", 3)}
</svg>`;
}
