// The logic behind /config: defaults, validation and the minimal-diff output, all driven by
// docs/config/config.schema.json. No React here, so scripts/configgen-selftest.mjs can run it.

export type Schema = {
  type?: string;
  description?: string;
  default?: unknown;
  minimum?: number;
  maximum?: number;
  properties?: Record<string, Schema>;
  additionalProperties?: boolean | Schema;
  items?: Schema;
  "x-class"?: string;
};

export const isRecordSchema = (s: Schema) => s.type === "object" && typeof s.additionalProperties === "object" && !s.properties;

const clone = <T,>(v: T): T => JSON.parse(JSON.stringify(v));
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);
const isObj = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v);

// The value a schema node has when nobody touches it.
export function defaultOf(s: Schema): unknown {
  if (s.type === "object") {
    if (isRecordSchema(s)) return {};
    const out: Record<string, unknown> = {};
    for (const [k, p] of Object.entries(s.properties ?? {})) out[k] = defaultOf(p);
    return out;
  }
  if (s.default !== undefined) return clone(s.default);
  if (s.type === "array") return [];
  if (s.type === "boolean") return false;
  if (s.type === "string") return "";
  return 0;
}

// Only what differs from the defaults, plus schemaVersion. A record entry is kept whole-key
// (even if all its fields are default, since the key itself adds a record) with only its changed fields.
export function minimalDiff(root: Schema, value: Record<string, unknown>): Record<string, unknown> {
  const out: Record<string, unknown> = { schemaVersion: root.properties?.schemaVersion?.default ?? 1 };
  for (const [k, v] of Object.entries(diffNode(root, value) ?? {})) if (k !== "schemaVersion") out[k] = v;
  return out;
}

function diffNode(s: Schema, v: unknown): unknown {
  if (s.type === "object" && isRecordSchema(s)) {
    const entry = s.additionalProperties as Schema;
    const out: Record<string, unknown> = {};
    for (const [key, ev] of Object.entries(isObj(v) ? v : {})) out[key] = diffNode(entry, ev) ?? {};
    return Object.keys(out).length ? out : undefined;
  }
  if (s.type === "object") {
    const out: Record<string, unknown> = {};
    for (const [k, p] of Object.entries(s.properties ?? {})) {
      const d = diffNode(p, isObj(v) ? v[k] : undefined);
      if (d !== undefined) out[k] = d;
    }
    return Object.keys(out).length ? out : undefined;
  }
  return same(v, defaultOf(s)) ? undefined : v;
}

// Live validation. Returns a map of path -> message; empty when the value is valid.
export function validate(s: Schema, v: unknown, path = ""): Record<string, string> {
  const errs: Record<string, string> = {};
  const at = path || "(root)";
  if (s.type === "object") {
    if (!isObj(v)) return { [at]: "must be an object" };
    if (isRecordSchema(s)) {
      for (const [key, ev] of Object.entries(v)) {
        if (!key.trim()) errs[`${path}.${key}`] = "key must not be empty";
        Object.assign(errs, validate(s.additionalProperties as Schema, ev, `${path}.${key}`));
      }
      return errs;
    }
    const props = s.properties ?? {};
    for (const k of Object.keys(v)) if (!(k in props) && s.additionalProperties === false) errs[`${path}.${k}`] = "unknown field";
    for (const [k, p] of Object.entries(props)) if (k in v) Object.assign(errs, validate(p, v[k], `${path}.${k}`));
    return errs;
  }
  if (s.type === "array") {
    if (!Array.isArray(v)) return { [at]: "must be a list" };
    v.forEach((item, i) => s.items && Object.assign(errs, validate(s.items, item, `${path}[${i}]`)));
    return errs;
  }
  if (s.type === "integer" || s.type === "number") {
    if (typeof v !== "number" || Number.isNaN(v)) return { [at]: "must be a number" };
    if (s.type === "integer" && !Number.isInteger(v)) return { [at]: "must be a whole number" };
    if (s.minimum !== undefined && v < s.minimum) return { [at]: `must be at least ${s.minimum}` };
    if (s.maximum !== undefined && v > s.maximum) return { [at]: `must be at most ${s.maximum}` };
    return errs;
  }
  if (s.type === "boolean" && typeof v !== "boolean") return { [at]: "must be true or false" };
  if (s.type === "string" && typeof v !== "string") return { [at]: "must be text" };
  return errs;
}
