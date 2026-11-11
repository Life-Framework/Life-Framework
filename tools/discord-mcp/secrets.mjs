// tools/discord-mcp/secrets.mjs - where the Discord tools read their secrets from.
//
// Both files live outside the repo, in ~/.life-framework/ (readable only by you):
//   discord-token       the bot token (same file the MCP server uses)
//   welcome-hook-url    the webhook URL the welcome relay posts events to; a secret,
//                       because anyone holding it can wake the orchestrator
// LF_DISCORD_TOKEN_FILE / LF_WELCOME_HOOK_FILE point at other files.
// Never commit either value or print the URL.

import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

export const GUILD_ID = process.env.DISCORD_GUILD_ID || "1444353039430582324";

export function tokenFile() {
  return process.env.LF_DISCORD_TOKEN_FILE || join(homedir(), ".life-framework", "discord-token");
}

export function hookFile() {
  return process.env.LF_WELCOME_HOOK_FILE || join(homedir(), ".life-framework", "welcome-hook-url");
}

function readSecret(file, what) {
  let text;
  try {
    text = readFileSync(file, "utf8").trim();
  } catch {
    throw new Error(`${what} not found: put it in ${file} (see tools/discord-mcp/README.md)`);
  }
  if (!text) throw new Error(`${what} file is empty: ${file}`);
  return text;
}

export const readToken = () => readSecret(tokenFile(), "Discord bot token");
export const readHookUrl = () => readSecret(hookFile(), "welcome webhook URL");
