/**
 * `texture_recolor` — create a recolored variant of an `.edds` texture.
 *
 * The one tool that actually solves "make the cement bag white": it decodes
 * the BC7 pixels with the engine's own Compressonator codec, applies color
 * transforms (hue / saturation / lightness / tint / whiten), re-encodes to
 * BC7, and writes a new `.edds` + `.meta` (fresh GUID) into the project so a
 * material or prefab can reference it as `{GUID}path`.
 *
 * Why FFI + the engine's DLL: `.edds` textures are BC7-compressed DDS files
 * (dxgiFormat 98/99). The engine can WRITE PNG/DDS from raw pixels but has NO
 * read-back API, and no npm package decodes BC7 — so Node decodes through
 * `CMP_DecodeBC7Block` / `CMP_EncodeBC7Block` from the same
 * `Compressonator_MD_DLL.dll` the engine ships. Round-trip is byte-faithful.
 *
 * Pipeline (per mip, per 4×4 block):
 *   parse `.edds` → decode BC7 block → apply ops on RGBA → encode BC7 block →
 *   rebuild `.edds` → write file + `.meta` (fresh GUID) → optional Workbench
 *   register (assigns GUID, creates proper meta).
 *
 * Source resolution order: project/workshop/core loose file, then game data
 * loose file, then game pak via PakVirtualFS (covers base-game textures).
 */

import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { dirname, resolve, sep } from "node:path";
import type { Config } from "../config.js";
import type { WorkbenchClient } from "../workbench/client.js";
import { formatConnectionStatus } from "../workbench/status.js";
import { validateProjectPath } from "../utils/safe-path.js";
import { resolveGameDataPath, findLooseFile, resolveAddonDir } from "../utils/game-paths.js";
import { PakVirtualFS } from "../pak/vfs.js";
import { generateGuid } from "../formats/guid.js";
import {
  parseEdds,
  extractMipPayloads,
  buildEdds,
  mipBlockCount,
  BC7_BLOCK_BYTES,
  dxgiFormatLabel,
} from "../textures/edds.js";
import { Compressonator } from "../textures/compressonator.js";
import { applyOps, type RecolorOp } from "../textures/recolor.js";

// ── Zod schema ───────────────────────────────────────────────────────────────

const opSchema = z.discriminatedUnion("op", [
  z.object({ op: z.literal("hue"), degrees: z.number().describe("Hue rotation in degrees") }),
  z.object({
    op: z.literal("saturation"),
    factor: z.number().describe("Saturation multiplier (0 = grayscale, 1 = unchanged)"),
  }),
  z.object({
    op: z.literal("lightness"),
    factor: z.number().describe("Lightness/brightness multiplier (1 = unchanged)"),
  }),
  z.object({
    op: z.literal("tint"),
    color: z.string().describe("Target color as #RRGGBB"),
    strength: z.number().describe("Blend strength 0..1 (1 = fully the target color)"),
  }),
  z.object({
    op: z.literal("whiten"),
    strength: z
      .number()
      .describe(
        "Whiten 0..1: desaturate to luminance then lift toward white (1 = pure white, keeps shading at lower values)",
      ),
  }),
]);

// ── Op conversion ────────────────────────────────────────────────────────────

type OpInput = z.infer<typeof opSchema>;

function toRecolorOps(ops: OpInput[]): RecolorOp[] {
  return ops.map((o) => {
    switch (o.op) {
      case "hue":
        return { hue_shift: o.degrees };
      case "saturation":
        return { saturation: o.factor };
      case "lightness":
        return { lightness: o.factor };
      case "tint":
        return { tint: o.color, tint_strength: o.strength };
      case "whiten":
        return { whiten: o.strength };
    }
  });
}

// ── Source resolution ────────────────────────────────────────────────────────

interface SourceHit {
  bytes: Buffer;
  label: string;
}

/** Strip a leading {16-hex} resource-GUID prefix. */
function stripGuidPrefix(p: string): string {
  const m = /^\{[A-Fa-f0-9]{16}\}(.*)$/.exec(p);
  return m ? m[1] : p;
}

/** Read a `.edds` from a loose project/workshop/core file, a game-data loose
 *  file, or a game pak. Returns null when nothing resolves. */
function resolveSource(source: string, config: Config): SourceHit | null {
  const bare = stripGuidPrefix(source);

  // 1. Project / workshop / core loose files
  const roots: Array<{ label: string; base: string | undefined }> = [
    { label: "project", base: config.projectPath },
    { label: "workshop", base: config.workshopPath },
    { label: "core", base: config.corePath },
  ];
  for (const { base } of roots) {
    if (!base || !existsSync(base)) continue;
    for (const mod of ["", config.defaultMod ?? ""]) {
      const candidate = mod
        ? resolve(base, mod, ...bare.split("/"))
        : resolve(base, ...bare.split("/"));
      if (existsSync(candidate) && candidate.startsWith(base + sep)) {
        return { bytes: readFileSync(candidate), label: `${candidate}` };
      }
    }
  }

  // 2. Game data loose files
  const gameData = resolveGameDataPath(config.gamePath);
  if (gameData) {
    const loose = findLooseFile(gameData, bare);
    if (loose) return { bytes: readFileSync(loose), label: `${loose} (game loose)` };
  }

  // 3. Game pak via PakVirtualFS
  const vfs = PakVirtualFS.get(config.gamePath);
  if (vfs && vfs.exists(bare)) {
    return { bytes: vfs.readFile(bare), label: `${bare} (game pak)` };
  }

  return null;
}

// ── The recolor pipeline ─────────────────────────────────────────────────────

/**
 * Recolor every mip of an `.edds` buffer. Each mip's BC7 blocks are decoded
 * to RGBA, transformed by `ops`, and re-encoded. Returns the rebuilt `.edds`.
 */
function recolorEdds(buf: Buffer, ops: RecolorOp[], coder: Compressonator, quality: number): Buffer {
  coder.createEncoder(quality);
  const info = parseEdds(buf);
  const payloads = extractMipPayloads(buf, info);

  const newPayloads: Buffer[] = [];
  // Payloads are in container order: ascending mip size, smallest mip FIRST.
  // Container slot i holds the mip whose block count is mipBlockCount(info,
  // mipCount-1-i). Process slots directly so extract/build stay in the same
  // order as the real file.
  for (let slot = 0; slot < info.mipCount; slot++) {
    const mip = info.mipCount - 1 - slot;
    const blocks = mipBlockCount(info, mip);
    const payload = payloads[slot];
    const out = Buffer.alloc(blocks * BC7_BLOCK_BYTES);

    for (let bi = 0; bi < blocks; bi++) {
      const block = payload.subarray(bi * BC7_BLOCK_BYTES, (bi + 1) * BC7_BLOCK_BYTES);
      const rgba = coder.decodeBlock(block);
      // Apply ops directly on the decoded RGBA doubles (0-255), in place.
      for (let p = 0; p < 16; p++) {
        const o = p * 4;
        const [r, g, b] = applyOps([rgba[o], rgba[o + 1], rgba[o + 2]], ops);
        rgba[o] = r;
        rgba[o + 1] = g;
        rgba[o + 2] = b;
      }
      const enc = coder.encodeBlock(rgba);
      enc.copy(out, bi * BC7_BLOCK_BYTES);
    }
    newPayloads.push(out);
  }

  return buildEdds(info, newPayloads);
}

// ── Meta generation ──────────────────────────────────────────────────────────

/**
 * Build the `.meta` contents for an `.edds` texture resource. The class is
 * `EDDSResourceClass` (confirmed from the game's resource class list).
 */
function textureMeta(guid: string, resourcePath: string): string {
  const platforms = ["PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS", "PS5"];
  const configs = platforms
    .map((p, i) => {
      const suffix = i === 0 ? "" : " : PC";
      return `  EDDSResourceClass ${p}${suffix} {\n  }`;
    })
    .join("\n");
  return [
    "MetaFileClass {",
    ` Name "{${guid}}${resourcePath}"`,
    " Configurations {",
    configs,
    " }",
    "}",
  ].join("\n");
}

// ── Tool registration ────────────────────────────────────────────────────────

export function registerTextureRecolor(
  server: McpServer,
  config: Config,
  wbClient: WorkbenchClient,
): void {
  server.registerTool(
    "texture_recolor",
    {
      description:
        "Create a recolored variant of an `.edds` texture (e.g. make the sandbag/cement bag white). " +
        "Decodes the BC7 pixels with the engine's own Compressonator codec, applies color ops " +
        "(hue / saturation / lightness / tint / whiten), re-encodes, and writes a new `.edds` + `.meta` " +
        "with a fresh GUID into the project addon so a material or prefab can reference it as {GUID}path. " +
        "Source may be a base-game texture (game pak or loose file) or a project texture. " +
        "Use asset_search first to find the source path.",
      inputSchema: {
        source: z
          .string()
          .describe(
            "Source `.edds` texture — a {GUID}path resource reference or bare path, e.g. " +
              "'Assets/Props/Military/Sandbags/Data/Sandbags_01_Wall_MLOD_BCR.edds'",
          ),
        output: z
          .string()
          .describe(
            "Destination path within the addon, relative to the addon root, ending in .edds " +
              "(e.g. 'Assets/Props/Military/Sandbags/Data/Sandbags_01_Wall_MLOD_BCR_White.edds')",
          ),
        ops: z
          .array(opSchema)
          .min(1)
          .describe(
            "Color operations applied in order. Example to make a brown sandbag white: " +
              '[{op:"whiten",strength:0.85}]. Others: hue {degrees}, saturation {factor}, ' +
              'lightness {factor}, tint {color:"#RRGGBB",strength}.',
          ),
        mod_name: z
          .string()
          .optional()
          .describe(
            "Addon folder name under ENFUSION_PROJECT_PATH (e.g. 'LifeFramework'). " +
              "Defaults to the configured default mod or the first addon found.",
          ),
        register: z
          .boolean()
          .default(true)
          .describe(
            "Register the new texture with Workbench after writing (assigns GUID, creates the .meta). " +
              "Requires Workbench to be running. Set false to write files only.",
          ),
        quality: z
          .number()
          .min(0)
          .max(1)
          .default(0.5)
          .describe("BC7 encode quality 0..1 (higher = slower, better). Default 0.5."),
      },
    },
    async ({ source, output, ops, mod_name, register, quality }) => {
      try {
        // ── Resolve source ─────────────────────────────────────────────
        const hit = resolveSource(source, config);
        if (!hit) {
          return {
            content: [
              {
                type: "text" as const,
                text:
                  `**Texture not found:** ${source}\n\n` +
                  "Searched project, workshop, core, game loose files, and game paks. " +
                  "Use asset_search to verify the path, or pass a {GUID}path." +
                  formatConnectionStatus(wbClient),
              },
            ],
            isError: true,
          };
        }

        // ── Resolve output addon dir ────────────────────────────────────
        const addonDir = resolveAddonDir(config.projectPath, mod_name ?? config.defaultMod);
        if (!addonDir) {
          return {
            content: [
              {
                type: "text" as const,
                text:
                  "Could not find addon directory. " +
                  (mod_name
                    ? `'${mod_name}' not found under ${config.projectPath}`
                    : `No addons found under ${config.projectPath}`) +
                  ". Provide mod_name matching the addon folder name.",
              },
            ],
            isError: true,
          };
        }

        let absOutput: string;
        try {
          absOutput = validateProjectPath(addonDir, output.replace(/\\/g, "/"));
        } catch {
          return {
            content: [{ type: "text" as const, text: `Invalid output path: ${output}` }],
            isError: true,
          };
        }
        if (existsSync(absOutput)) {
          return {
            content: [
              {
                type: "text" as const,
                text: `Output already exists: ${absOutput}\nChoose a different output path.`,
              },
            ],
            isError: true,
          };
        }

        // ── Find Compressonator DLL ─────────────────────────────────────
        const dllPath = Compressonator.findDll(config.workbenchPath);
        if (!dllPath) {
          return {
            content: [
              {
                type: "text" as const,
                text:
                  `Compressonator DLL not found under ${config.workbenchPath}/Workbench/. ` +
                  "This tool needs the engine's BC7 codec (Compressonator_MD_DLL.dll).",
              },
            ],
            isError: true,
          };
        }

        // ── Parse + recolor ─────────────────────────────────────────────
        const info = parseEdds(hit.bytes);
        const coder = new Compressonator(dllPath);
        try {
          const recolorOps = toRecolorOps(ops);
          const rebuilt = recolorEdds(hit.bytes, recolorOps, coder, quality);
          const mip0Bytes = rebuilt.length;
          void mip0Bytes;

          // ── Write file + meta ─────────────────────────────────────────
          mkdirSync(dirname(absOutput), { recursive: true });
          writeFileSync(absOutput, rebuilt);

          const resourcePath = output.replace(/\\/g, "/").replace(/^\.\//, "");
          const guid = generateGuid();
          const metaPath = `${absOutput}.meta`;
          writeFileSync(metaPath, textureMeta(guid, resourcePath));

          const opSummary = ops
            .map((o) =>
              o.op === "tint"
                ? `tint→${o.color}@${o.strength}`
                : `${o.op}(${"degrees" in o ? o.degrees : "factor" in o ? o.factor : o.strength})`,
            )
            .join(", ");

          const lines: string[] = [
            "**Texture recolored**",
            `- **Source:** ${source} (${hit.label})`,
            `- **Format:** ${info.width}×${info.height}, ${info.mipCount} mips, ` +
              `${dxgiFormatLabel(info.dxgiFormat)}`,
            `- **Ops:** ${opSummary}`,
            `- **Saved to:** ${absOutput}`,
            `- **Meta:** ${metaPath}`,
            `- **GUID:** ${guid}`,
            "",
            `Reference it as **{${guid}}${resourcePath}** in a material or prefab.`,
            "",
            "**Next steps:**",
            `1. Point the target material's albedo (BCR) slot at {${guid}}${resourcePath}, or`,
            "2. Duplicate the source material (.emat) and swap its texture reference.",
          ];

          // ── Optional Workbench register ───────────────────────────────
          if (register) {
            try {
              const regResp = await wbClient.call<{ status: string; message?: string }>(
                "EMCP_WB_Resources",
                { action: "register", path: absOutput, buildRuntime: false },
                { timeout: 30000 },
              );
              if (regResp.status === "ok") {
                lines.push(
                  "",
                  "Registered with Workbench — the resource now has an official GUID " +
                    "(Workbench may rewrite the .meta with its own GUID; read it back via wb_resources getInfo).",
                );
              } else {
                lines.push(
                  "",
                  `Warning: Workbench registration returned: ${regResp.message ?? JSON.stringify(regResp)}`,
                );
              }
            } catch (e) {
              const msg = e instanceof Error ? e.message : String(e);
              lines.push(
                "",
                `Note: file written but Workbench registration failed (${msg}). ` +
                  "The .meta GUID is already valid for manual use.",
              );
            }
          }

          return {
            content: [{ type: "text" as const, text: lines.join("\n") }],
          };
        } finally {
          coder.shutdown();
        }
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        return {
          content: [
            {
              type: "text" as const,
              text: `Error recoloring texture: ${msg}${formatConnectionStatus(wbClient)}`,
            },
          ],
          isError: true,
        };
      }
    },
  );
}