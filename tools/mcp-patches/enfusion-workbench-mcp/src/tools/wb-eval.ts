// lf-register: registerWbEval(server, wbClient)
//
// wb_eval: compile and run a snippet of Enforce Script inside the live
// Workbench (handler: mod/.../EMCP_WB_Eval.c). Local Life Framework patch,
// reapplied by `tools\cli mcp update enfusion-workbench`. The handler lives
// only in tools/mcp-patches/ and is never part of addons/.

import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { WorkbenchClient } from "../workbench/client.js";
import { formatConnectionStatus } from "../workbench/status.js";
import { isHandlerError, handlerErrorResponse } from "../workbench/response.js";

/** Matches EMCP_WB_Eval.MAX_CODE_LENGTH in the handler. */
export const WB_EVAL_MAX_CODE = 32768;

export function registerWbEval(server: McpServer, client: WorkbenchClient): void {
  server.registerTool(
    "wb_eval",
    {
      description:
        "Compile and run a snippet of Enforce Script in the live Workbench and return the string it returns. " +
        "For ad-hoc, read-only inspection (WorldEditorAPI, ResourceManager, ResourceDatabase); anything you will run again belongs in a committed handler or tool. " +
        "The snippet is the body of a `string` function; the locals `we` (WorldEditor), `api` (WorldEditorAPI, null in play mode) and `rm` (ResourceManager) are ready. " +
        "End with `return <string>;` (a missing return yields \"\"). " +
        "Example: `return api.GetEditorEntityCount().ToString();`. " +
        "Enforce Script has no ternary (?:) and no null-coalescing. " +
        "WORLD EDITS: wrap every WorldEditorAPI mutation in `api.BeginEntityAction(\"<label>\");` ... `api.EndEntityAction();` so it enters undo history; never edit outside that pair. " +
        `Limit: ${WB_EVAL_MAX_CODE} characters. ` +
        "Each new snippet costs about 11 MB the engine never frees; an identical snippet reuses its compiled module, and after 100 fresh compiles the tool refuses until Workbench restarts. " +
        "A null dereference or other VME inside the snippet WEDGES Workbench (this and every later call time out until it restarts): guard every pointer, e.g. `if (!api) return \"no api\";`. " +
        "Returns status ok with the result, or compile_error with the snippet line and compiler text (the session stays healthy). runtime_error appears only when the call fails without a VME. See docs/wb-eval.md.",
      inputSchema: {
        code: z
          .string()
          .min(1)
          .max(WB_EVAL_MAX_CODE)
          .describe("Enforce Script statements forming the body of a function returning string"),
      },
    },
    async ({ code }) => {
      try {
        const result = await client.call<Record<string, unknown>>("EMCP_WB_Eval", { code });
        if (isHandlerError(result)) {
          return handlerErrorResponse(result, client, "wb_eval");
        }
        const status = String(result.status ?? "");
        const footer = formatConnectionStatus(client);
        if (status === "ok") {
          return {
            content: [{ type: "text" as const, text: String(result.result ?? "") }],
            structuredContent: {
              status,
              result: String(result.result ?? ""),
              cached: Boolean(result.cached),
              compiles: Number(result.compiles ?? 0),
            },
          };
        }
        if (status === "compile_error") {
          const errorLine = Number(result.errorLine ?? 0);
          const errorText = String(result.errorText ?? "");
          return {
            content: [
              {
                type: "text" as const,
                text: `compile_error at snippet line ${errorLine}: ${errorText}${footer}`,
              },
            ],
            structuredContent: { status, errorLine, errorText },
            isError: true,
          };
        }
        const message = String(result.message ?? "unknown failure");
        return {
          content: [{ type: "text" as const, text: `${status || "error"}: ${message}${footer}` }],
          structuredContent: { status: status || "error", message },
          isError: true,
        };
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        return {
          content: [{ type: "text" as const, text: `wb_eval failed: ${msg}${formatConnectionStatus(client)}` }],
          isError: true,
        };
      }
    },
  );
}
