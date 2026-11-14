// lf-register: registerWbPreview(server, wbClient, config)
/**
 * `wb_preview` - look at one resource, or the edit-mode viewport, without play mode.
 *
 * Drives EMCP_WB_Preview in two phases. The handler never sleeps (a Sleep()
 * inside a NetApiHandler raises "Cannot stop main thread" and blocks the render
 * thread that writes screenshots), so the waiting happens here in Node:
 *   1. open the resource in Resource Manager and/or SetCamera in the World Editor
 *   2. wait `settleMs`
 *   3. MakeScreenshot to a unique stem under `$logs:`
 *   4. poll until the file exists, is non-empty and its size is stable
 *   5. BMP -> PNG, optional downscale to `maxPx`, return image block + path
 *
 * Spec: tools wb-tooling-spec item 3 (Resource Manager preview + edit-mode SetCamera).
 */

import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import type { WorkbenchClient } from "../workbench/client.js";
import type { Config } from "../config.js";
import { formatConnectionStatus } from "../workbench/status.js";
import {
  decodeImage,
  downscale,
  encodePng,
  isPng,
  lumaStdDev,
  waitForStableFile,
} from "../utils/preview-image.js";

const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

/** Directories where `$logs:` can resolve (the engine does not create subfolders): the logs root and its newest session dirs. */
function previewDirs(logsPath: string | undefined): string[] {
  if (!logsPath) return [];
  const dirs = [logsPath];
  try {
    const subs = readdirSync(logsPath, { withFileTypes: true })
      .filter((e) => e.isDirectory())
      .map((e) => ({ name: e.name, mtime: statSync(join(logsPath, e.name)).mtimeMs }))
      .sort((a, b) => b.mtime - a.mtime)
      .slice(0, 3);
    for (const s of subs) dirs.push(join(logsPath, s.name));
  } catch {
    /* logs dir not readable */
  }
  return dirs;
}

/** First file whose name starts with `stem` (the render thread may append .bmp/.png). */
function locateShot(logsPath: string | undefined, stem: string): string | null {
  for (const dir of previewDirs(logsPath)) {
    try {
      const hit = readdirSync(dir).find((f) => f.startsWith(stem));
      if (hit) return join(dir, hit);
    } catch {
      /* dir not there */
    }
  }
  return null;
}

function text(t: string, isError = false) {
  return { content: [{ type: "text" as const, text: t }], ...(isError ? { isError: true } : {}) };
}

export function registerWbPreview(server: McpServer, client: WorkbenchClient, config: Config): void {
  server.registerTool(
    "wb_preview",
    {
      description:
        "Capture the Workbench viewport without play mode and return it as an image plus a saved PNG path. " +
        "`resource` ({GUID}path or $Addon:path of a .et/.xob/.emat) is opened in Resource Manager first, so a " +
        "prop can be inspected with no world loaded. `position` + `direction` ('x y z') move the World Editor " +
        "edit-mode camera (WorldEditorAPI.SetCamera) before the shot; the handler returns the camera position it " +
        "read back. With neither, the current view is captured. The wait for the render thread happens in Node " +
        "(`settleMs`, then a stable-file poll up to `timeoutMs`), never inside Workbench.",
      inputSchema: {
        resource: z
          .string()
          .optional()
          .describe("Resource to open in Resource Manager: '{GUID}path' or '$Addon:path'"),
        position: z.string().optional().describe("Edit-mode camera position, 'x y z'"),
        direction: z
          .string()
          .optional()
          .describe("Edit-mode camera look direction, 'x y z' (normalised; must be non-zero)"),
        settleMs: z.number().optional().describe("Wait after open/SetCamera before the shot (default 800)"),
        timeoutMs: z.number().optional().describe("Max wait for the screenshot file (default 15000)"),
        maxPx: z
          .number()
          .optional()
          .describe("Downscale so the longer side is at most this many pixels (default: keep full size)"),
        outDir: z
          .string()
          .optional()
          .describe("Directory for the saved PNG (default: <tmp>/lf-wb-preview)"),
      },
    },
    async (args) => {
      const settleMs = args.settleMs ?? 800;
      const timeoutMs = args.timeoutMs ?? 15000;
      const wantsCamera = Boolean(args.position || args.direction);
      if (wantsCamera && !(args.position && args.direction)) {
        return text("position and direction must be given together", true);
      }

      try {
        const notes: string[] = [];

        // Phase 1: queue the state change.
        if (args.resource) {
          const open = await client.call<Record<string, unknown>>("EMCP_WB_Preview", {
            action: "open",
            resource: args.resource,
          });
          if (open.status !== "ok") {
            return text(
              `Open failed: ${open.message ?? "unknown error"}${formatConnectionStatus(client)}`,
              true,
            );
          }
          notes.push(`opened ${args.resource}`);
        }
        if (wantsCamera) {
          const cam = await client.call<Record<string, unknown>>("EMCP_WB_Preview", {
            action: "setCamera",
            position: args.position,
            direction: args.direction,
          });
          if (cam.status !== "ok") {
            return text(
              `SetCamera failed: ${cam.message ?? "unknown error"}${formatConnectionStatus(client)}`,
              true,
            );
          }
          notes.push(`camera ${cam.position ?? "(no read-back)"}`);
        }

        // Settle in Node, not in Workbench.
        if (settleMs > 0) await sleep(settleMs);

        // Phase 2: screenshot to a unique stem.
        const stem = `lfp${Date.now().toString(36)}${Math.random().toString(36).slice(2, 6)}`;
        const shot = await client.call<Record<string, unknown>>("EMCP_WB_Preview", {
          action: "screenshot",
          path: `$logs:${stem}`,
        });
        if (shot.status !== "queued") {
          return text(
            `Screenshot rejected: ${shot.message ?? "unknown error"}${formatConnectionStatus(client)}`,
            true,
          );
        }

        const file = await waitForStableFile(() => locateShot(config.logsPath, stem), timeoutMs);
        if (!file) {
          return text(
            `No stable screenshot file for stem ${stem} within ${timeoutMs} ms ` +
              `(looked under ${previewDirs(config.logsPath).join(", ") || "(no logsPath)"}).` +
              formatConnectionStatus(client),
            true,
          );
        }

        // Convert, downscale, save.
        const raw = readFileSync(file);
        const decoded = decodeImage(raw);
        let png: Buffer;
        let spread = "n/a";
        let size = "?";
        if (decoded) {
          spread = lumaStdDev(decoded).toFixed(1);
          const scaled = args.maxPx ? downscale(decoded, args.maxPx) : decoded;
          size = `${scaled.width}x${scaled.height}`;
          // An unscaled PNG from the engine is passed through untouched.
          png = isPng(raw) && scaled === decoded ? raw : encodePng(scaled);
        } else if (isPng(raw)) {
          png = raw;
          size = `${raw.readUInt32BE(16)}x${raw.readUInt32BE(20)}`;
        } else {
          return text(`Screenshot at ${file} is neither BMP nor PNG (${raw.length} bytes).`, true);
        }

        const outDir = args.outDir ?? join(tmpdir(), "lf-wb-preview");
        mkdirSync(outDir, { recursive: true });
        const outPath = join(outDir, `${stem}.png`);
        writeFileSync(outPath, png);

        const lines = [
          `**Preview captured**: \`${outPath}\` (${size}, luma stddev ${spread})`,
          ...notes.map((n) => `- ${n}`),
          `- source: \`${file}\``,
        ];
        return {
          content: [
            { type: "text" as const, text: lines.join("\n") + formatConnectionStatus(client) },
            { type: "image" as const, data: png.toString("base64"), mimeType: "image/png" },
          ],
        };
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        return text(`Error capturing preview: ${msg}${formatConnectionStatus(client)}`, true);
      }
    },
  );
}
