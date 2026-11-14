/**
 * Pure helpers for the wb_resources tool (LF patch, spec items 1-2):
 * request building and response shaping. No Workbench I/O here, so it is unit-tested.
 *
 * List parameters go to the Enforce handler as '|' separated strings, because the
 * handler's JsonApiStruct request does not parse JSON arrays.
 */

export const BROWSE_DEFAULT_LIMIT = 200;
export const BROWSE_MAX_LIMIT = 1000;
export const DEFAULT_CONFIGURATION = "PC";
export const DEFAULT_WAIT_MS = 5000;
/** Base NET API timeout (ms) before per-path wait time is added. */
export const BASE_TIMEOUT_MS = 15000;

export interface ResourcesInput {
  action: "register" | "rebuild" | "getInfo" | "open" | "browse";
  path?: string;
  paths?: string[];
  buildRuntime?: boolean;
  configuration?: string;
  waitMs?: number;
  rootPath?: string;
  extensions?: string[];
  filters?: string[];
  recursive?: boolean;
  limit?: number;
}

/** Join list items for the handler: trimmed, empty dropped, '|' inside an item is not allowed. */
export function joinList(items: string[] | undefined): string {
  if (!items) return "";
  return items
    .map((s) => s.trim())
    .filter((s) => s.length > 0 && !s.includes("|"))
    .join("|");
}

/** Strip a leading dot from an extension: ".et" -> "et". */
export function normalizeExtension(ext: string): string {
  return ext.trim().replace(/^\./, "");
}

/** Clamp the browse limit to [1, 1000]; default 200. */
export function clampLimit(limit: number | undefined): number {
  if (limit === undefined || !Number.isFinite(limit)) return BROWSE_DEFAULT_LIMIT;
  const n = Math.floor(limit);
  if (n < 1) return BROWSE_DEFAULT_LIMIT;
  return Math.min(n, BROWSE_MAX_LIMIT);
}

/** Clamp waitMs to >= 0; default 5000. 0 skips waiting. */
export function clampWaitMs(waitMs: number | undefined): number {
  if (waitMs === undefined || !Number.isFinite(waitMs)) return DEFAULT_WAIT_MS;
  return Math.max(0, Math.floor(waitMs));
}

/** All paths for a register/rebuild call: `paths[]` plus the single `path`, de-duplicated. */
export function collectPaths(input: Pick<ResourcesInput, "path" | "paths">): string[] {
  const out: string[] = [];
  for (const p of [...(input.paths ?? []), ...(input.path ? [input.path] : [])]) {
    const t = p.trim();
    if (t && !out.includes(t)) out.push(t);
  }
  return out;
}

/** Params for the browse action. */
export function buildBrowseParams(input: ResourcesInput): Record<string, unknown> {
  return {
    action: "browse",
    rootPath: (input.rootPath ?? input.path ?? "").trim(),
    extensions: joinList((input.extensions ?? []).map(normalizeExtension)),
    filters: joinList(input.filters),
    recursive: input.recursive ?? true,
    limit: clampLimit(input.limit),
  };
}

/** Params for register / rebuild (batch). */
export function buildBatchParams(input: ResourcesInput): Record<string, unknown> {
  const params: Record<string, unknown> = {
    action: input.action,
    paths: joinList(collectPaths(input)),
    configuration: (input.configuration ?? "").trim() || DEFAULT_CONFIGURATION,
    waitMs: clampWaitMs(input.waitMs),
  };
  if (input.buildRuntime !== undefined) params.buildRuntime = input.buildRuntime;
  return params;
}

/** NET API timeout for a batch: base + waitMs per path (the handler waits for each in turn). */
export function batchTimeoutMs(pathCount: number, waitMs: number): number {
  return BASE_TIMEOUT_MS + Math.max(1, pathCount) * Math.max(0, waitMs);
}

/**
 * Object items of a JSON array. The Enforce JsonApiStruct packs `StartObject("")` inside
 * `StartArray` as `["", {...}, "", {...}]` (a blank string before each object, seen in a real
 * Workbench), so anything that is not an object is dropped.
 */
export function objectItems(value: unknown): Record<string, unknown>[] {
  if (!Array.isArray(value)) return [];
  return value.filter(
    (v): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v),
  );
}

export interface BrowseEntry {
  name: string;
  path: string;
  resourceName: string;
}

export interface BrowseResult {
  status: string;
  count: number;
  limit: number;
  truncated?: boolean;
  entries: BrowseEntry[];
  message?: string;
}

/** Shape a raw handler payload as `{ status, count, limit, entries }`. Never throws. */
export function shapeBrowseResult(raw: Record<string, unknown>, limit: number): BrowseResult {
  const entries: BrowseEntry[] = objectItems(raw.entries).map((r) => {
    return {
      name: String(r.name ?? ""),
      path: String(r.path ?? ""),
      resourceName: String(r.resourceName ?? ""),
    };
  });
  const result: BrowseResult = {
    status: typeof raw.status === "string" ? raw.status : "ok",
    count: entries.length,
    limit: typeof raw.limit === "number" && raw.limit > 0 ? raw.limit : limit,
    entries,
  };
  if (raw.truncated === true || raw.truncated === 1) result.truncated = true;
  if (entries.length === 0 && typeof raw.message === "string" && raw.message) {
    result.message = raw.message;
  }
  return result;
}

export interface BatchOutcome {
  path: string;
  reason?: string;
}

export interface BatchResult {
  status: string;
  processed: BatchOutcome[];
  failed: BatchOutcome[];
  configuration: string;
  message?: string;
}

function shapeOutcomes(value: unknown): BatchOutcome[] {
  return objectItems(value).map((r) => {
    const o: BatchOutcome = { path: String(r.path ?? "") };
    if (typeof r.reason === "string" && r.reason) o.reason = r.reason;
    return o;
  });
}

/** Shape a raw handler payload as `{ status, processed, failed, configuration }`. */
export function shapeBatchResult(raw: Record<string, unknown>): BatchResult {
  const result: BatchResult = {
    status: typeof raw.status === "string" ? raw.status : "ok",
    processed: shapeOutcomes(raw.processed),
    failed: shapeOutcomes(raw.failed),
    configuration:
      typeof raw.configuration === "string" && raw.configuration
        ? raw.configuration
        : DEFAULT_CONFIGURATION,
  };
  if (typeof raw.message === "string" && raw.message) result.message = raw.message;
  return result;
}
