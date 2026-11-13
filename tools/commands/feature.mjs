// tools\cli feature new|check: the feature scaffold (tools/feature/new.mjs).
export const help = "feature new <Area> <Name>     scaffold a feature: script, tests, proof, strings, docs rows, PR body (--kind logic|world|ui|item, --issue N); `feature check` lists what is left";

export async function main(args, ctx) {
  return (await import("../feature/new.mjs")).main(args, ctx);
}
