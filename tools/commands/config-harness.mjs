export const help = "config-harness [--stage parse|validate|all] [--json]   run the config fixtures (tools/test/config-fixtures) through config-check, offline";

export async function main(args) {
  return (await import("../config/harness.mjs")).main(args ?? []);
}
