# Host subcommands

One file per `tools\cli host <sub>` subcommand that is not built into
`tools/cli.mjs`. `host <sub>` loads `tools/host/commands/<sub>.mjs` when `<sub>`
is not `start`, `stop`, `status`, `backup`, `preflight` or `restore`, and
`host help` lists every file here. A new host subcommand is one new file and
never edits `tools/cli.mjs` (AGENTS.md golden rule 10).

A file is named `<sub>.mjs` (lowercase letters, digits, `-`) and exports:

```js
// One line shown by `host help`: usage, then what it does.
export const help = "hello [name]   say hello";

// args: the words after `host <sub>`. ctx: { root, hostDir } (repo root,
// the server host folder). Return an exit code (0 ok) or a promise of one.
export async function main(args, ctx) {
  console.log("hello", args.join(" "));
  return 0;
}
```

Rules: log through `LF_Debug`-style greppable prefixes, bound every external
call (AGENTS.md "Nothing waits forever"), and return a non-zero code on failure.
`tools/validation/host-commands-selftest.mjs` proves the fallthrough with a
fixture in a temp dir (`LF_HOST_COMMANDS_DIR`).
