import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { WorkbenchClient } from "../workbench/client.js";
import { formatConnectionStatus, requireEditMode } from "../workbench/status.js";
import { isHandlerError, handlerErrorResponse } from "../workbench/response.js";
import {
  buildBatchParams,
  buildBrowseParams,
  batchTimeoutMs,
  clampLimit,
  clampWaitMs,
  collectPaths,
  shapeBatchResult,
  shapeBrowseResult,
} from "./wb-resources-params.js";

export function registerWbResources(server: McpServer, client: WorkbenchClient): void {
  server.registerTool(
    "wb_resources",
    {
      description:
        "Manage Workbench resources. Register new resources, rebuild resources, get resource info, open a resource in its editor, or browse the LIVE Resource Database. " +
        "browse: query the live ResourceDatabase (not the offline PAK index), so freshly registered or generated resources show up with their real {GUID}path; returns JSON { status, count, limit, entries: [{ name, path, resourceName }] }. An unknown rootPath returns count 0. " +
        "register/rebuild: batch via paths[], then wait (waitMs) for the Resource Database to acknowledge each file so the next step cannot race the asset; returns JSON { status, processed, failed, configuration }. Paths are '$Addon:relative/path' or verified absolute paths.",
      inputSchema: {
        action: z
          .enum(["register", "rebuild", "getInfo", "open", "browse"])
          .describe(
            "Action: register (add resources to DB), rebuild (re-import already registered resources), getInfo (resource metadata), open (open in editor), browse (live Resource Database search)",
          ),
        path: z
          .string()
          .optional()
          .describe(
            "Single resource path ('$Addon:path' or absolute). Required for getInfo/open; for register/rebuild a single-path shortcut for paths. For browse it is used as rootPath when rootPath is not given.",
          ),
        paths: z
          .array(z.string())
          .optional()
          .describe(
            "register/rebuild: batch of resource paths ('$Addon:relative/path' or verified absolute). A malformed or missing path lands in failed[] without aborting the rest.",
          ),
        configuration: z
          .string()
          .optional()
          .describe("rebuild: build configuration (default \"PC\")."),
        waitMs: z
          .number()
          .int()
          .min(0)
          .optional()
          .describe(
            "register/rebuild: max ms per processed path to wait for the Resource Database to acknowledge the file (default 5000; 0 skips waiting).",
          ),
        buildRuntime: z
          .boolean()
          .optional()
          .describe(
            "Build runtime data during register (slower but ensures assets are ready)",
          ),
        rootPath: z
          .string()
          .optional()
          .describe(
            "browse: search root in exact-path form, e.g. '$LifeFramework:Prefabs'. Empty searches every loaded addon.",
          ),
        extensions: z
          .array(z.string())
          .optional()
          .describe("browse: file extensions without dots, e.g. [\"et\", \"xob\", \"emat\"]."),
        filters: z
          .array(z.string())
          .optional()
          .describe("browse: substring filters applied to the resource path."),
        recursive: z.boolean().optional().describe("browse: recurse into subfolders (default true)."),
        limit: z
          .number()
          .int()
          .min(1)
          .optional()
          .describe("browse: max entries returned (default 200, capped at 1000)."),
      },
    },
    async (input) => {
      const { action } = input;
      const path = input.path ?? "";
      try {
        // Mutating actions require edit mode
        if (action === "register" || action === "rebuild") {
          const modeErr = requireEditMode(client, `${action} resource`);
          if (modeErr) {
            return {
              content: [{ type: "text" as const, text: modeErr + formatConnectionStatus(client) }],
            };
          }
        }

        if (action === "browse") {
          const limit = clampLimit(input.limit);
          const raw = await client.call<Record<string, unknown>>(
            "EMCP_WB_Resources",
            buildBrowseParams(input),
          );
          if (isHandlerError(raw)) {
            return handlerErrorResponse(raw, client, `Error browsing resources`);
          }
          return {
            content: [
              {
                type: "text" as const,
                text: JSON.stringify(shapeBrowseResult(raw, limit), null, 2),
              },
            ],
          };
        }

        if (action === "getInfo" && !path) {
          return {
            content: [{ type: "text" as const, text: "path is required for getInfo." }],
            isError: true,
          };
        }

        if (action === "getInfo") {
          // Use built-in GetResourceInfo handler
          const result = await client.call<Record<string, unknown>>("GetResourceInfo", {
            path,
          });

          const lines = ["**Resource Info**\n"];
          lines.push(`- **Path:** ${path}`);
          if (result.guid) lines.push(`- **GUID:** ${result.guid}`);
          if (result.type) lines.push(`- **Type:** ${result.type}`);
          if (result.size !== undefined) lines.push(`- **Size:** ${result.size}`);
          if (result.lastModified) lines.push(`- **Modified:** ${result.lastModified}`);
          if (result.dependencies && Array.isArray(result.dependencies)) {
            lines.push(`\n### Dependencies (${result.dependencies.length})`);
            for (const dep of result.dependencies) {
              lines.push(`- ${dep}`);
            }
          }

          // Fallback for unknown response shapes
          const knownKeys = new Set([
            "guid",
            "type",
            "size",
            "lastModified",
            "dependencies",
            "path",
          ]);
          for (const [key, val] of Object.entries(result)) {
            if (!knownKeys.has(key) && val !== undefined) {
              lines.push(`- **${key}:** ${typeof val === "object" ? JSON.stringify(val) : val}`);
            }
          }

          return {
            content: [
              { type: "text" as const, text: lines.join("\n") + formatConnectionStatus(client) },
            ],
          };
        }

        if (action === "register" || action === "rebuild") {
          const targets = collectPaths(input);
          if (targets.length === 0) {
            return {
              content: [{ type: "text" as const, text: `path or paths is required for ${action}.` }],
              isError: true,
            };
          }
          const params = buildBatchParams(input);
          const raw = await client.call<Record<string, unknown>>("EMCP_WB_Resources", params, {
            timeout: batchTimeoutMs(targets.length, clampWaitMs(input.waitMs)),
          });
          if (isHandlerError(raw)) {
            return handlerErrorResponse(raw, client, `Error on ${action} (${targets.length} path(s))`);
          }
          const shaped = shapeBatchResult(raw);
          return {
            content: [{ type: "text" as const, text: JSON.stringify(shaped, null, 2) }],
          };
        }

        // open
        if (!path) {
          return {
            content: [{ type: "text" as const, text: "path is required for open." }],
            isError: true,
          };
        }
        const result = await client.call<Record<string, unknown>>("EMCP_WB_Resources", {
          action,
          path,
        });
        if (isHandlerError(result)) {
          return handlerErrorResponse(result, client, `Error managing resource "${path}" (${action})`);
        }

        return {
          content: [
            {
              type: "text" as const,
              text: `**Opened resource: ${path}**${result.message ? `

${result.message}` : ""}${formatConnectionStatus(client)}`,
            },
          ],
        };
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        return {
          content: [
            {
              type: "text" as const,
              text: `Error managing resource "${path}" (${action}): ${msg}${formatConnectionStatus(client)}`,
            },
          ],
          isError: true,
        };
      }
    },
  );
}
