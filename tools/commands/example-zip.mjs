// tools\cli example-zip [--final] [--out <dir>]   the release download built from server/example (#296)
import { main as run } from "../release/example-zip.mjs";

export const help = "example-zip [--final] [--out <dir>]   pack server/example into LifeFramework-ExampleServer.zip (a -DRAFT zip while Workshop ids are placeholders; --final refuses them)";

export async function main(args, { root }) {
  return run(args, { root });
}
