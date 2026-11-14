import { describe, it, expect } from "vitest";
import {
  BROWSE_DEFAULT_LIMIT,
  BROWSE_MAX_LIMIT,
  DEFAULT_CONFIGURATION,
  DEFAULT_WAIT_MS,
  batchTimeoutMs,
  buildBatchParams,
  buildBrowseParams,
  clampLimit,
  clampWaitMs,
  collectPaths,
  joinList,
  normalizeExtension,
  objectItems,
  shapeBatchResult,
  shapeBrowseResult,
} from "../wb-resources-params.js";

describe("joinList / normalizeExtension", () => {
  it("trims, drops empties and items containing the separator", () => {
    expect(joinList([" et ", "", "xob", "a|b"])).toBe("et|xob");
    expect(joinList(undefined)).toBe("");
  });
  it("strips a leading dot", () => {
    expect(normalizeExtension(".et")).toBe("et");
    expect(normalizeExtension("emat")).toBe("emat");
  });
});

describe("clampLimit / clampWaitMs", () => {
  it("defaults to 200 and caps at 1000", () => {
    expect(clampLimit(undefined)).toBe(BROWSE_DEFAULT_LIMIT);
    expect(clampLimit(0)).toBe(BROWSE_DEFAULT_LIMIT);
    expect(clampLimit(50)).toBe(50);
    expect(clampLimit(99999)).toBe(BROWSE_MAX_LIMIT);
  });
  it("waitMs defaults to 5000, 0 skips, negatives clamp to 0", () => {
    expect(clampWaitMs(undefined)).toBe(DEFAULT_WAIT_MS);
    expect(clampWaitMs(0)).toBe(0);
    expect(clampWaitMs(-5)).toBe(0);
    expect(clampWaitMs(1200)).toBe(1200);
  });
});

describe("buildBrowseParams", () => {
  it("joins extensions and filters, recursive defaults true", () => {
    const p = buildBrowseParams({
      action: "browse",
      rootPath: "$LifeFramework:Prefabs",
      extensions: [".et", "xob"],
      filters: ["Shop"],
    });
    expect(p).toEqual({
      action: "browse",
      rootPath: "$LifeFramework:Prefabs",
      extensions: "et|xob",
      filters: "Shop",
      recursive: true,
      limit: 200,
    });
  });
  it("falls back to path as rootPath and honours recursive=false and the cap", () => {
    const p = buildBrowseParams({ action: "browse", path: "$A:B", recursive: false, limit: 5000 });
    expect(p.rootPath).toBe("$A:B");
    expect(p.recursive).toBe(false);
    expect(p.limit).toBe(1000);
  });
});

describe("collectPaths / buildBatchParams", () => {
  it("merges paths[] and path without duplicates", () => {
    expect(collectPaths({ paths: ["$A:x.edds", " $A:y.edds "], path: "$A:x.edds" })).toEqual([
      "$A:x.edds",
      "$A:y.edds",
    ]);
    expect(collectPaths({})).toEqual([]);
  });
  it("applies defaults: configuration PC, waitMs 5000", () => {
    const p = buildBatchParams({ action: "rebuild", paths: ["$A:x.edds"] });
    expect(p).toEqual({
      action: "rebuild",
      paths: "$A:x.edds",
      configuration: DEFAULT_CONFIGURATION,
      waitMs: DEFAULT_WAIT_MS,
    });
  });
  it("passes explicit values, waitMs 0 and buildRuntime", () => {
    const p = buildBatchParams({
      action: "register",
      path: "$A:a.fbx",
      paths: ["$A:b.fbx"],
      configuration: "HEADLESS",
      waitMs: 0,
      buildRuntime: true,
    });
    expect(p).toMatchObject({
      paths: "$A:b.fbx|$A:a.fbx",
      configuration: "HEADLESS",
      waitMs: 0,
      buildRuntime: true,
    });
  });
});

describe("batchTimeoutMs", () => {
  it("scales with path count and waitMs", () => {
    expect(batchTimeoutMs(3, 5000)).toBe(15000 + 15000);
    expect(batchTimeoutMs(0, 0)).toBe(15000);
  });
});

describe("objectItems", () => {
  it("drops the blank strings the Enforce JSON packer puts between array objects", () => {
    expect(objectItems(["", { a: 1 }, "", { b: 2 }])).toEqual([{ a: 1 }, { b: 2 }]);
    expect(objectItems(undefined)).toEqual([]);
  });
});

describe("shapeBrowseResult", () => {
  it("handles the real packed shape: blank items and truncated as 1", () => {
    const r = shapeBrowseResult(
      {
        status: "ok",
        limit: 3,
        truncated: 1,
        entries: ["", { name: "A.et", path: "$X:A.et", resourceName: "{1}A.et" }, ""],
      },
      3,
    );
    expect(r.count).toBe(1);
    expect(r.truncated).toBe(true);
  });
  it("shapes entries and sets count from them", () => {
    const r = shapeBrowseResult(
      {
        status: "ok",
        limit: 200,
        entries: [{ name: "A.et", path: "$X:Prefabs/A.et", resourceName: "{AAAA}Prefabs/A.et" }],
      },
      200,
    );
    expect(r).toEqual({
      status: "ok",
      count: 1,
      limit: 200,
      entries: [{ name: "A.et", path: "$X:Prefabs/A.et", resourceName: "{AAAA}Prefabs/A.et" }],
    });
  });
  it("empty result is ok with count 0 and carries the message", () => {
    const r = shapeBrowseResult({ status: "ok", message: "No resources matched" }, 200);
    expect(r.status).toBe("ok");
    expect(r.count).toBe(0);
    expect(r.entries).toEqual([]);
    expect(r.message).toBe("No resources matched");
  });
  it("does not carry state between calls", () => {
    const first = shapeBrowseResult({ entries: [{ name: "a", path: "p", resourceName: "r" }] }, 10);
    const second = shapeBrowseResult({}, 10);
    expect(first.count).toBe(1);
    expect(second.count).toBe(0);
    expect(second.entries).toEqual([]);
  });
});

describe("shapeBatchResult", () => {
  it("splits processed and failed", () => {
    const r = shapeBatchResult({
      status: "ok",
      configuration: "PC",
      processed: [{ path: "C:/a.edds", reason: "acknowledged" }],
      failed: [{ path: "bad", reason: "relative path needs an $Addon: prefix" }],
    });
    expect(r.processed).toEqual([{ path: "C:/a.edds", reason: "acknowledged" }]);
    expect(r.failed).toEqual([{ path: "bad", reason: "relative path needs an $Addon: prefix" }]);
    expect(r.configuration).toBe("PC");
  });
  it("defaults missing arrays and configuration", () => {
    const r = shapeBatchResult({});
    expect(r).toEqual({ status: "ok", processed: [], failed: [], configuration: "PC" });
  });
});
