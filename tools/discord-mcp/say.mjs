// node tools/discord-mcp/say.mjs <channelId> <replyToMessageId|-> <textFile>
// Posts the text file as the bot in a channel, optionally as a reply. Prints "<status> <messageId>".
import { readFileSync } from "node:fs";
import { readToken } from "./secrets.mjs";

const [channel, reply, file] = process.argv.slice(2);
if (!channel || !file) {
  console.log("usage: node tools/discord-mcp/say.mjs <channelId> <replyToMessageId|-> <textFile>");
  process.exit(2);
}
const body = { content: readFileSync(file, "utf8").trim(), allowed_mentions: { parse: ["users"] } };
if (reply && reply !== "-") body.message_reference = { message_id: reply, fail_if_not_exists: false };
const r = await fetch(`https://discord.com/api/v10/channels/${channel}/messages`, {
  method: "POST",
  headers: { Authorization: `Bot ${readToken()}`, "Content-Type": "application/json" },
  body: JSON.stringify(body),
});
console.log(r.status, r.ok ? (await r.json()).id : await r.text());
process.exit(r.ok ? 0 : 1);
