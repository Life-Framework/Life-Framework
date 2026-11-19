// Shared by validate-sign-materials and its selftest: what a sign prefab's material slots must look like.

// Vanilla sign bodies: face slot -> the far-LOD slot the base prefab leaves on the generic placeholder.
export const SIGN_BODY_LOD_SLOTS = {
  SignGeneric_01: "SignGeneric_01_MLOD",
  SignWall_01_115x67: "SignWall_01_115x67_MLOD",
  SignWall_01_90X45: "SignWall_01_90X45_MLOD",
  SignCommercial_01: "MLOD",
  SignLarge_01: "SignLarge_01_MLOD",
};

// Every MaterialAssignClass in a prefab: { line, source, assigned path (no GUID), block id }.
export function materialAssignments(text) {
  const out = [];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    if (!/^\s*MaterialAssignClass\b/.test(lines[i])) continue;
    let source = "";
    let assigned = "";
    for (let j = i + 1; j < lines.length && j < i + 6; j++) {
      const s = lines[j].match(/^\s*SourceMaterial\s+"([^"]*)"/);
      if (s) source = s[1];
      const a = lines[j].match(/^\s*AssignedMaterial\s+"(?:\{[0-9A-Fa-f]{16}\})?([^"]*)"/);
      if (a) assigned = a[1];
      if (/^\s*\}\s*$/.test(lines[j])) break;
    }
    out.push({ line: i + 1, source, assigned });
  }
  return out;
}

// `readEmat(path)` returns the .emat text or null when no addon has it.
export function signMaterialProblems(text, readEmat) {
  const problems = [];
  const all = materialAssignments(text);
  const assignedSources = new Set(all.map((a) => a.source));
  const checked = new Set();
  for (const a of all) {
    const lod = SIGN_BODY_LOD_SLOTS[a.source];
    if (lod && !assignedSources.has(lod)) {
      problems.push({ line: a.line, message: `assigns ${a.source} but not ${lod}: the far LOD keeps the generic placeholder and draws black` });
    }
    if (!a.assigned || checked.has(a.assigned)) continue;
    checked.add(a.assigned);
    // Vanilla materials live in the game paks and are not ours to read: only our own sign .emats are checked.
    if (!/^Assets\/Signs\//i.test(a.assigned)) continue;
    const emat = readEmat(a.assigned);
    if (emat === null) {
      problems.push({ line: a.line, message: `${a.assigned} is in no addon: the slot resolves to a placeholder` });
      continue;
    }
    const hasMap = /\b(BCRMap|BCMap|ColorMap)\s+"[^"]+"/.test(emat);
    const colour = emat.match(/^\s*Color\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)/m);
    const black = !colour || (Number(colour[1]) === 0 && Number(colour[2]) === 0 && Number(colour[3]) === 0);
    if (!hasMap && black) problems.push({ line: a.line, message: `${a.assigned} has no texture or $rendertarget and a black or missing Color` });
  }
  return problems;
}
