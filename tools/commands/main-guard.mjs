// tools\cli main-guard: the post-merge guard on main (tools/main-guard.mjs, docs/worktrees.md "Main guard").
export const help = "main-guard [start|stop|status]   post-merge guard: full suite, build, placement audit, pre-move e2e on main; bisects and reverts what broke it";

export async function main(args, { root }) {
  return (await import("../main-guard.mjs")).main(args, { root });
}
