# Wipe policy for the alpha

This page covers the alpha server, "Life Framework | Everon 1989 (Alpha)". It is password-gated, so you join with the server password. For the rest of the server rules, see the [player FAQ](../player-faq.md).

**The server can be wiped at any time during the alpha.** A wipe can come at any point while the alpha runs, with no fixed schedule. Every wipe is announced on Discord at least 24 hours before it happens. No wipe comes sooner than that.

**A wipe clears all progress.** Everything in your save goes: balances, keys, jail time and the rest. You start over on the next session. Read the Discord announcement before you invest time in a long build.

**Progress is kept from the public launch.** Progress made from the public launch onward is not wiped by alpha wipes. A wipe after the public launch would be a separate, deliberate decision, and it would be announced the same way.

## For staff

The maintainer is the only admin, so the maintainer runs every step below. The commands are planned and not all of them exist yet. Run `tools\cli host help` to see what is merged.

1. **Announce.** `host announce --wipe` drafts the Discord notice. It refuses a wipe less than 24 hours out, so the 24 hour notice is enforced by the announce command (planned).
2. **Back up.** The wipe step writes a backup of the save before it clears anything. `host backup` takes a backup by hand today.
3. **Clear.** `host wipe --confirm` clears the save after the backup is written. It needs the typed confirmation (planned).

Do not clear a save before the announcement has gone out.
