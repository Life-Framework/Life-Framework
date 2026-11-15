# Run a password alpha

A password alpha is a server that only people you invited can join. The gate is one join password in
`server.json`. There are no accounts to manage and no codes to redeem: whoever has the password can join.
This page covers setting it, sharing it, changing it, wiping the world with notice, and opening the
server by removing the password. It works for any server on the core, with any pack or none.

For the first install, read [Install a server](install.md) first. A flagship server's own alpha rules
are a worked example of this page: [the alpha page](../community/alpha.md).

## Set the password

In your `server.json`, put the password in `game.password`:

```json
"game": {
  "name": "My Life Framework server",
  "password": "pick-something-long",
  "passwordAdmin": "a-different-one"
}
```

- `game.password` is the **join** password, asked from every player when they join.
- `game.passwordAdmin` is the **admin** login (`#login <password>` in game). It is a different secret.
  Never give the admin password to testers, and never reuse one for the other.
- Use letters, digits and dashes. A `"` or a `\` in the value needs JSON escaping, so avoid them.
  Spaces are legal in the file, but a password a tester has to copy is easier with none.
- An empty string, `""`, means no password: the server is open.

The server reads `server.json` when it starts. Restart it (Ctrl+C so it writes the shutdown save, then
the start script) and the password is live.

## Keep it out of the repository

The password lives on the machine that runs the server and nowhere else. Never put it in a pull
request, an issue, a Discord channel, a screenshot or a doc.

- Run the server from a folder outside any git checkout, for example `C:\LFServer`. The
  [example server folder](../../server/example/README.md) ships with `"password": ""`: copy it out and
  edit the copy, not the file in the repo.
- If your server folder is a git repository, ignore the file: add `server.json` to `.gitignore`, then
  check with `git check-ignore server.json`, which prints the name when it is ignored. Keep a copy of the
  template with the secrets blanked if you want the rest of the config under version control.
- If a password ever lands in a commit, treat it as leaked: change it (below) and then clean the history.

## Share it

Send the password with the invite, by direct message, one person at a time.

- Say what the server is called, how to find it, and what to expect: sessions, the chance of a wipe,
  where to report what broke.
- Ask testers not to pass it on. If a friend wants in, they ask you.
- Keep a short list of who you gave it to. You need it to know whom to tell when it changes.

The password is a gate against drive-by joins, not strong security: one tester can still forward it.
That is the reason to rotate it.

## Rotate it

Change the password when it leaks, when a tester leaves, or on a schedule you set.

1. Pick a restart time and say it in your channel ahead of time. A restart disconnects everyone.
2. Stop the server with Ctrl+C so it saves.
3. Edit `game.password` in `server.json`.
4. Start it again (with `-loadSessionSave`, the usual start; see [Install a server](install.md)).
5. Send the new password to the people on your list.

Nothing changes in anyone's mod setup: only the string they type at the prompt. The old password stops
working at the restart.

## Wipe with notice

An alpha save can be thrown away. Players trust that more when you give notice, so set a rule and keep it
(a flagship server announces every wipe at least 24 hours ahead).

1. **Announce first.** Say when the wipe happens and that it clears all progress.
2. **Back up.** Stop the server with Ctrl+C, then copy the whole `profile/` folder somewhere safe. You
   can get the old town back from it.
3. **Start without `-loadSessionSave`.** Without the flag the engine starts a new playthrough even when
   saves exist ([how saves load](../persistence.md)). The log says
   `started a NEW session while N save point(s) exist`; on a wipe that line is expected
   ([log lines](log-lines.md)). The next autosave makes the fresh town the latest save.
4. **Put the flag back.** Every restart after the wipe starts with `-loadSessionSave` again, or the next
   restart wipes the town you just started.

Do not clear a save before the announcement has gone out. A wiped save keeps the password gate as it was.

A wipe rule written for players is a short page of its own; the flagship's is the [wipe policy](../community/wipe-policy.md).

## Open the server

When you are ready for anyone to join:

1. Announce the opening and say whether progress is kept from that day. Decide it before you announce,
   and do not wipe afterwards without telling people.
2. Stop the server with Ctrl+C.
3. Set `"password": ""` in `server.json`. Leave `passwordAdmin` set.
4. Start it again with `-loadSessionSave`, join without a password yourself, and check it works.

If it goes badly, the same edit with a password closes it again.

## Checklist

- [ ] `game.password` set, `game.passwordAdmin` set to something else
- [ ] `server.json` is outside a git checkout, or ignored by it
- [ ] The password went out by direct message, and you wrote down who has it
- [ ] A wipe is announced before it happens, and the `profile/` backup exists
- [ ] Every start after a wipe passes `-loadSessionSave`
