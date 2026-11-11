// node tools/discord-mcp/dm.mjs <userId> <textFile>
// Opens a DM with the user and sends the text file as the bot. Prints "dm <channelId> <status>".
import { readFileSync } from "node:fs";
import { readToken } from "./secrets.mjs";

const [user, file] = process.argv.slice(2);
if (!user || !file) {
  console.log("usage: node tools/discord-mcp/dm.mjs <userId> <textFile>");
  process.exit(2);
}
const headers = { Authorization: `Bot ${readToken()}`, "Content-Type": "application/json" };
const dm = await (
  await fetch("https://discord.com/api/v10/users/@me/channels", {
    method: "POST",
    headers,
    body: JSON.stringify({ recipient_id: user }),
  })
).json();
if (!dm.id) {
  console.log("no dm", JSON.stringify(dm));
  process.exit(1);
}
const r = await fetch(`https://discord.com/api/v10/channels/${dm.id}/messages`, {
  method: "POST",
  headers,
  body: JSON.stringify({ content: readFileSync(file, "utf8").trim() }),
});
console.log("dm", dm.id, r.status, r.ok ? "" : await r.text());
process.exit(r.ok ? 0 : 1);
