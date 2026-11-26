// Shared by validate-us-english and its selftest: player-readable text that is not US English.
//
// The mod is US English everywhere a player can read it, signs and painted textures included (maintainer,
// 2026-11-22: "It's still set in the region, but players must be able to read the signs"). Place names
// (Saint-Philippe, Everon) and people's names stay as they are; everything else is US English with US spelling.
//
// Three sources are read:
//   - string tables: the Target_en_us value of every row of addons/*/Language/*.st (Ids and Comments are not text);
//   - sign configs: any quoted field under Configs/Signs that is literal text (not a `#Key`, not an id);
//   - texture text: every string literal in a tools/assets/<slug>/*.py script that paints text onto a texture
//     (calls `.text(`); `texture-text.json` beside the script lists them for a reviewer (`--update` writes it).
//
// A hit is a whole word (or phrase) from FRENCH or BRITISH, any case, accents optional where noted.

// Common French words that end up on signs, doors, shop names and notices.
export const FRENCH = [
  "boulangerie", "boulanger", "pharmacie", "tabac", "tabac-presse", "banque", "ouvert", "ouverte", "ouverture",
  "fermé", "fermée", "fermeture", "sortie", "entrée", "poste", "postes", "billets", "à vendre", "a vendre", "à louer",
  "mairie", "gendarmerie", "boucherie", "charcuterie", "épicerie", "quincaillerie", "coiffeur", "coiffure",
  "meubles", "scierie", "verger", "entrepôt", "propriété", "privée", "réservé", "réservée", "école", "bienvenue",
  "horaires", "interdit", "interdite", "distributeur", "achat", "zone militaire", "vacances", "librairie",
  "pâtisserie", "poissonnerie", "fromagerie", "droguerie", "drogerie", "mercerie", "bientôt",
  "voir le panneau", "hôtel de ville", "caisse", "soldes", "prix", "francs", "bonjour", "merci", "Mme", "Mlle",
];

// British spellings with a US form, and British words a US reader trips on.
export const BRITISH = {
  licence: "license", licences: "licenses", organisation: "organization", organisations: "organizations",
  harbour: "harbor", harbours: "harbors", colour: "color", colours: "colors", favourite: "favorite",
  neighbour: "neighbor", neighbours: "neighbors", armoury: "armory", armour: "armor", armoured: "armored",
  offence: "offense", offences: "offenses", tyre: "tire", tyres: "tires", kerb: "curb", metre: "meter",
  metres: "meters", kilometre: "kilometer", kilometres: "kilometers", litre: "liter", litres: "liters",
  centre: "center", cheque: "check", cheques: "checks", fibreglass: "fiberglass", moulded: "molded",
  recognised: "recognized", petrol: "gas", "taxi rank": "taxi stand", newsagent: "newsstand", "café": "cafe",
};

// Generated forms: -ise verbs and -our nouns (each with its common endings) map to the US spelling.
const ISE = ["authoris", "organis", "recognis", "realis", "apologis", "specialis", "minimis", "maximis", "customis", "utilis", "summaris", "categoris", "criticis", "finalis", "legalis", "memoris", "normalis", "penalis", "prioritis", "sympathis", "visualis", "capitalis", "civilis", "emphasis", "familiaris", "hospitalis", "idealis", "mobilis", "neutralis", "personalis", "standardis", "stabilis", "sterilis", "subsidis"];
for (const stem of ISE) {
  if (stem === "emphasis") continue; // "emphasis" is the noun, the same in US English
  const us = stem.slice(0, -2) + "iz";
  for (const end of ["e", "es", "ed", "ing", "ation", "ations"]) BRITISH[stem.slice(0, -2) + "is" + end] = us + end;
}
for (const stem of ["labo", "hono", "behavio", "flavo", "humo", "rumo", "vapo", "odo", "savo", "endeavo", "parlo", "splendo", "valo", "fervo", "rigo", "vigo", "candou"]) {
  if (stem === "candou") continue;
  for (const end of ["ur", "urs", "ured", "uring", "urable"]) BRITISH[stem + end] = stem + "r" + end.slice(2);
}
Object.assign(BRITISH, {
  grey: "gray", greys: "grays", greyed: "grayed", greying: "graying", "lay-by": "pull-off", jerrycan: "jerry can",
  "driving licence": "driver's license", "driving license": "driver's license", "driving licences": "driver's licenses",
  "driving licenses": "driver's licenses", theatre: "theater", fibre: "fiber", calibre: "caliber", lustre: "luster",
  sabre: "saber", spectre: "specter", sombre: "somber", meagre: "meager", manoeuvre: "maneuver", defence: "defense",
  pretence: "pretense", practise: "practice", practised: "practiced", plough: "plow", mould: "mold", storey: "story",
  aluminium: "aluminum", sceptic: "skeptic", sceptical: "skeptical", tonne: "ton", tonnes: "tons", lorry: "truck",
  lorries: "trucks", pavement: "sidewalk", bonnet: "hood", windscreen: "windshield", spanner: "wrench",
  spanners: "wrenches", dustbin: "trash can", postcode: "ZIP code", fortnight: "two weeks", whilst: "while",
  amongst: "among", learnt: "learned", spelt: "spelled", jewellery: "jewelry", cosy: "cozy", pyjamas: "pajamas",
  programme: "program", programmes: "programs", artefact: "artifact", artefacts: "artifacts", sulphur: "sulfur",
  gaol: "jail", gaoler: "jailer", draught: "draft", judgement: "judgment", ageing: "aging", tinned: "canned",
  casualty: "emergency", mum: "mom", trousers: "pants", rubbish: "trash", queue: "line", queues: "lines",
  autumn: "fall", holdall: "duffel bag", forecourt: "gas station lot", cancelled: "canceled", cancelling: "canceling",
  travelling: "traveling", travelled: "traveled", labelled: "labeled", modelled: "modeled", modelling: "modeling",
});

const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const wordRe = (list) => new RegExp(`(?<![\\p{L}\\p{N}_])(${list.map(esc).join("|")})(?![\\p{L}\\p{N}_])`, "giu");
const FRENCH_RE = wordRe(FRENCH);
const BRITISH_RE = wordRe(Object.keys(BRITISH));

// [{ word, kind: "french"|"british", hint }] for one piece of text.
export function findNonUs(text) {
  const out = [];
  for (const m of text.matchAll(FRENCH_RE)) out.push({ word: m[1], kind: "french", hint: "write it in English" });
  for (const m of text.matchAll(BRITISH_RE)) out.push({ word: m[1], kind: "british", hint: `US: ${BRITISH[m[1].toLowerCase()]}` });
  return out;
}

// Rows of a .st file: [{ id, text, line }], the Target_en_us value of each.
export function stRows(text) {
  const rows = [];
  for (const m of text.matchAll(/Id "([^"]+)"\s*\n(\s*)Target_en_us "((?:[^"\\]|\\.)*)"/g)) {
    const line = text.slice(0, m.index).split("\n").length + 1;
    rows.push({ id: m[1], text: m[3], line });
  }
  return rows;
}

// Literal text in a sign config: quoted values of text fields that are not `#Key` references.
const SIGN_TEXT_FIELD = /^\s*(m_s(?:Title|What|How|Needs|Need|Rules|Next|Special\d*|Text|Sub\w*|Caption|Line\d*))\s+"([^"]*)"/;
export function signConfTexts(text) {
  const out = [];
  text.split("\n").forEach((l, i) => {
    const m = SIGN_TEXT_FIELD.exec(l);
    if (m && m[2] && !m[2].startsWith("#")) out.push({ field: m[1], text: m[2], line: i + 1 });
  });
  return out;
}

// String literals of a texture build script that paints text. Layout keywords and file names are left out.
const LAYOUT_WORDS = new Set(["LEFT", "RIGHT", "CENTER", "CENTRE", "MESH", "FACE", "OBJECT", "EDIT", "VERT", "EDGE"]);
// A script that lays out printed text calls `<canvas>.text("...")`; a drawing library (paperlib.py) only defines it.
export function paintsText(src) {
  return /\.text\(\s*f?["']/.test(src);
}
export function textureTexts(src) {
  const out = [];
  // Docstrings describe the script; they are blanked (newlines kept, so line numbers hold).
  const lines = src.replace(/("""|''')[\s\S]*?\1/g, (m) => m.replace(/[^\n]/g, " ")).split("\n");
  lines.forEach((l, i) => {
    const code = l.replace(/(^|\s)#.*$/, "");
    for (const m of code.matchAll(/(?<![A-Za-z])(?:f)?"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'/g)) {
      const s = m[1] ?? m[2];
      if (!s || !/[A-Za-z]{2}/.test(s)) continue;
      if (LAYOUT_WORDS.has(s)) continue;
      if (/^[a-z_][a-z0-9_]*$/.test(s)) continue; // identifiers, keys, face names
      if (!/\s/.test(s) && /[\\/]|\.(png|json|py|xob|emat|edds|tif|blend|glb|fbx|ttf|otf)$/i.test(s)) continue; // paths and files
      out.push({ text: s, line: i + 1 });
    }
  });
  return out;
}
