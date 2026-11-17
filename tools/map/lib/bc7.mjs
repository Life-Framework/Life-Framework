// tools/map/lib/bc7.mjs - the mean colour of each 4x4 BC7 block, from the block's endpoints only.
// A terrain tile's supertexture is BC7; the map needs one colour per block, not the pixels, so the
// partition and index tables are never read: the mean of every endpoint stands for the block.
const MODES = [
  { ns: 3, pb: 4, rb: 0, isb: 0, cb: 4, ab: 0, epb: 1, spb: 0 },
  { ns: 2, pb: 6, rb: 0, isb: 0, cb: 6, ab: 0, epb: 0, spb: 1 },
  { ns: 3, pb: 6, rb: 0, isb: 0, cb: 5, ab: 0, epb: 0, spb: 0 },
  { ns: 2, pb: 6, rb: 0, isb: 0, cb: 7, ab: 0, epb: 1, spb: 0 },
  { ns: 1, pb: 0, rb: 2, isb: 1, cb: 5, ab: 6, epb: 0, spb: 0 },
  { ns: 1, pb: 0, rb: 2, isb: 0, cb: 7, ab: 8, epb: 0, spb: 0 },
  { ns: 1, pb: 0, rb: 0, isb: 0, cb: 7, ab: 7, epb: 1, spb: 0 },
  { ns: 2, pb: 6, rb: 0, isb: 0, cb: 5, ab: 5, epb: 1, spb: 0 },
];

const expand = (v, bits) => (v << (8 - bits)) | (v >> (2 * bits - 8));

// Writes [r,g,b] of the block at byte offset `o` of `buf` into out[k..k+2].
export function blockMean(buf, o, out, k) {
  const b0 = buf[o];
  if (b0 === 0) {
    out[k] = out[k + 1] = out[k + 2] = 0;
    return;
  }
  let mode = 0;
  while (!((b0 >> mode) & 1)) mode++;
  const m = MODES[mode];
  let pos = mode + 1;
  const read = (n) => {
    let v = 0;
    for (let i = 0; i < n; i++, pos++) v |= ((buf[o + (pos >> 3)] >> (pos & 7)) & 1) << i;
    return v;
  };
  read(m.pb);
  const rot = read(m.rb);
  read(m.isb);
  const ne = m.ns * 2;
  const e = [];
  for (let i = 0; i < ne; i++) e.push([0, 0, 0, 255]);
  for (let c = 0; c < 3; c++) for (let i = 0; i < ne; i++) e[i][c] = read(m.cb);
  if (m.ab) for (let i = 0; i < ne; i++) e[i][3] = read(m.ab);
  const pbits = [];
  if (m.epb) for (let i = 0; i < ne; i++) pbits.push(read(1));
  else if (m.spb) for (let s = 0; s < m.ns; s++) {
    const p = read(1);
    pbits.push(p, p);
  }
  const hasP = pbits.length > 0;
  let r = 0, g = 0, bl = 0;
  for (let i = 0; i < ne; i++) {
    const ch = [0, 0, 0, 0];
    for (let c = 0; c < 4; c++) {
      const bits = c < 3 ? m.cb : m.ab;
      if (c === 3 && !m.ab) { ch[3] = 255; continue; }
      const v = hasP ? (e[i][c] << 1) | pbits[i] : e[i][c];
      ch[c] = expand(v, hasP ? bits + 1 : bits);
    }
    if (rot) { const t = ch[3]; ch[3] = ch[rot - 1]; ch[rot - 1] = t; }
    r += ch[0]; g += ch[1]; bl += ch[2];
  }
  out[k] = Math.round(r / ne);
  out[k + 1] = Math.round(g / ne);
  out[k + 2] = Math.round(bl / ne);
}
