# Invite-only alpha

How the flagship alpha, "Life Framework | Everon 1989 (Alpha)", is opened to a
few people first, how a player asks to join, and what has to be true before
the server opens to everyone. Issue: #555. Decisions: the launch decisions of
2026-11-09 and 2026-11-14 in [the M4 outline](../roadmap/m4-outline.md).

## How access works

The server password is the whole gate. There is no account list and no code to
redeem: anyone who has the password can join.

- The maintainer hands the password out with the invite.
- The password lives on the host and nowhere in this repository. It is never
  posted in a public channel or written in a doc.
- The maintainer changes it on the host when it leaks or when a tester leaves.
  A changed password needs no change to anyone else's setup; testers ask for
  the new one.
- Opening the server to the public is removing the password (#555). That is the
  last step of the gate below, and it is the maintainer's call.

The alpha is early. The server runs in announced sessions, and it can be wiped
at any time with 24 hours' notice on Discord. Read `docs/community/wipe-policy.md`
before you invest time in a long build.

## Asking for an invite

1. Join the [Life Framework Discord](https://discord.gg/MjMexJteqz).
2. Tell staff you want in: post in #general, or message a Community Manager.
   Say your Steam name, because your character uses it.
3. Install the game and the mod first. [Open the playtest](../playtest.md)
   covers the download and the first join.
4. A staff member sends the invite message below in a direct message. If you
   do not hear back, ask again in #general. Invites go out in small batches so
   the server is not full on the first night.

Staff never ask for your Steam login. The invite carries a password for this
server only.

## The invite message

Staff send this by direct message. The maintainer fills in the password; it is
never pasted anywhere public.

```
You are invited to the Life Framework alpha.

Server: Life Framework | Everon 1989 (Alpha)
Password: <the maintainer fills this in>

How to join:
1. Install the Life Framework mod (https://life-framework.org/playtest).
2. Open Arma Reforger, find the server in the browser or join it from the
   playtest launcher, and enter the password when asked.
3. On your first join, fill in form R-89 with your name and age.

What to expect:
- The server runs in sessions announced on Discord, not around the clock.
- It is an alpha. The save can be wiped at any time with at least 24 hours'
  notice on Discord, and a wipe clears all progress.
- Please do not share the password. Ask staff if a friend wants in.
- Report what broke in #playtest-feedback.
```

## The opening gate

The alpha opens to the public when all four hold:

- [ ] **Sessions at the player target.** Several sessions in a row reach the
  player target (about 40 players).
- [ ] **No save loss.** No session in the alpha lost or corrupted a save, and a
  restart brought every player's progress back.
- [ ] **No blocking regression.** No open issue labelled `regression` stops a
  player from joining, playing a job, or keeping progress.
- [ ] **Trial moderators in place.** The staff roster, `server/flagship/staff-roster.json`,
  names the trial moderators before the public opening. They come from the
  playtesters, on probation (#313), and they work from `docs/community/moderator-guide.md`.
  At the start of the alpha only the maintainer is an administrator, so this item
  is not a condition for the first invites.

When the four are checked, the maintainer removes the password and announces the
opening on Discord. Progress is kept from that day, as in the wipe policy.

## Who does what

| Step | Who |
| --- | --- |
| Hands out the password, changes it, removes it | Maintainer |
| Sends invites and answers requests | Staff |
| Announces a wipe, 24 hours ahead | Maintainer (`docs/community/wipe-policy.md`) |
| Ticks the gate | Maintainer, from the session logs and the issue list |
