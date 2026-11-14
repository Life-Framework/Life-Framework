// tools\cli workshop boot-check <coreId> <packId>   the clean-server proof for the first Workshop upload (#371)
export const help = "workshop boot-check <coreId> <packId> [--host lf-cloud] [--no-client] [--plan] [--force-boot] [--budget <s>]   boot a CLEAN dedicated server on the box from Workshop ids only (no local addons, empty cache), join a client, report pass/fail";

export async function main(args, ctx) {
  const [sub, ...rest] = args;
  if (sub === "boot-check") return (await import("../release/boot-check.mjs")).main(rest, ctx);
  console.log("usage: tools\cli workshop boot-check <coreId> <packId> [flags]");
  return sub ? 2 : 0;
}
