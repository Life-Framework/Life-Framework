// tools\cli usage: token spend per agent run from the Claude Code session logs (tools/agents/usage.mjs).
export const help = "usage [--since 7d|2026-11-01] [--top N] [--type lf-hard] [--leads] [--detail] [--json]   cost-weighted tokens, context and tool shares per agent run";

export async function main(args) {
  return (await import("../agents/usage.mjs")).main(args);
}
