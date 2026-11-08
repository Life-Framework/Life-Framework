// Builds src/generated/support.json from content/support.json and content/costs.json.
//
// The support page shows a way to give only when support.json says `"enabled": true` and that
// method is filled in. The build fails rather than publish something wrong: a malformed or
// mistyped address, a non-https card link, enabled with nothing to show, or a
// .github/FUNDING.yml that disagrees with the flag. QR codes are drawn here, at build time,
// so no third-party QR service ever sees the address.

import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import qrcode from "qrcode-generator";

const B58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

function base58(s) {
  let n = 0n;
  for (const ch of s) {
    const i = B58.indexOf(ch);
    if (i < 0) return null;
    n = n * 58n + BigInt(i);
  }
  const bytes = [];
  while (n > 0n) {
    bytes.unshift(Number(n % 256n));
    n /= 256n;
  }
  for (const ch of s) {
    if (ch !== "1") break;
    bytes.unshift(0);
  }
  return Buffer.from(bytes);
}

const sha256 = (b) => crypto.createHash("sha256").update(b).digest();

// A TRON address is base58check: 0x41, 20 bytes, then the first 4 bytes of sha256(sha256(those 21)).
export function validTron(a) {
  if (!/^T[1-9A-HJ-NP-Za-km-z]{33}$/.test(a)) return false;
  const b = base58(a);
  if (!b || b.length !== 25 || b[0] !== 0x41) return false;
  return sha256(sha256(b.subarray(0, 21))).subarray(0, 4).equals(b.subarray(21));
}

// Shape only; the mixed-case EIP-55 checksum needs keccak, which node does not ship.
export const validEvm = (a) => /^0x[0-9a-fA-F]{40}$/.test(a);

const NETWORKS = {
  TRON: { valid: validTron, explorer: (a) => `https://tronscan.org/#/address/${a}`, explorerName: "Tronscan" },
  Ethereum: { valid: validEvm, explorer: (a) => `https://etherscan.io/address/${a}`, explorerName: "Etherscan" },
};

// One SVG path of dark modules, in module units, with a 4-module quiet zone.
function qrPath(text) {
  const q = qrcode(0, "M");
  q.addData(text);
  q.make();
  const n = q.getModuleCount();
  let d = "";
  for (let r = 0; r < n; r++) for (let c = 0; c < n; c++) if (q.isDark(r, c)) d += `M${c + 4} ${r + 4}h1v1h-1z`;
  return { size: n + 8, path: d };
}

export function buildSupport({ site, repo }) {
  const read = (p) => JSON.parse(fs.readFileSync(path.join(site, "content", p), "utf8"));
  const support = read("support.json");
  const costs = read("costs.json");
  const fail = (msg) => {
    throw new Error(`content/support.json: ${msg} (docs/community/support.md)`);
  };

  const enabled = support.enabled === true;
  let card = null;
  if (support.card?.url) {
    if (!/^https:\/\/[^\s/]+\.[^\s]+$/.test(support.card.url)) fail(`card.url "${support.card.url}" is not an https link`);
    if (!support.card.name) fail("card.url is set but card.name is empty");
    card = { name: support.card.name, url: support.card.url };
  }

  const wallets = [];
  for (const c of support.crypto ?? []) {
    if (!c.address) continue;
    const net = NETWORKS[c.network];
    if (!net) fail(`unknown network "${c.network}"; known: ${Object.keys(NETWORKS).join(", ")}`);
    if (!net.valid(c.address)) fail(`"${c.address}" is not a valid ${c.network} address; check it character by character`);
    wallets.push({ ...c, explorer: net.explorer(c.address), explorerName: net.explorerName, qr: qrPath(c.address) });
  }

  if (enabled && !card && wallets.length === 0) fail('"enabled" is true but no card link or address is filled in');

  const funding = fs.existsSync(path.join(repo, ".github", "FUNDING.yml"));
  if (enabled && !funding) fail('"enabled" is true but .github/FUNDING.yml is missing; add it in the same change');
  if (!enabled && funding) fail('"enabled" is false but .github/FUNDING.yml exists, so GitHub shows a Sponsor button to a closed page');

  for (const i of costs.items) {
    if (i.amount !== null && typeof i.amount !== "number") throw new Error(`content/costs.json: "${i.what}" amount must be a number or null`);
    if (!["month", "year"].includes(i.period)) throw new Error(`content/costs.json: "${i.what}" period must be month or year`);
  }

  return {
    enabled,
    card: enabled ? card : null,
    crypto: enabled ? wallets : [],
    costs,
  };
}
