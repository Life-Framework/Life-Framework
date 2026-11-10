# Outreach kit

Everything the maintainer needs to post about Life Framework in other
communities, ready to paste. Issues: #572 (the kit), #573 ([FAQ](faq.md)),
#574 ([tracker](tracker.md)), #570 (the [pictures](#pictures)). Epic: #566.
Why this exists: our Discord is small and the release gates need testers
([M2.5 outline](../../roadmap/m2-5-outline.md)).

## Who does what

- **The maintainer posts, as themselves, where they have permission.** Our
  agents never post in another server and never use a user account for it. A
  self-bot breaks Discord's terms and puts the maintainer's account at risk.
- **Agents write and update the files here,** keep the [tracker](tracker.md),
  and write the weekly summary.
- **Read the community's own rules first, then ask the mods before posting.**
  The rules in each file were gathered from outside, and some could not be
  read without joining. Those are marked UNVERIFIED. Treat the community's
  current rules as the truth, and fix the file when you read them.

## Rules for every post

From [discord.md](../discord.md): no hype, no dates, no promises. In practice:

- Say what is built and what is not. "A small test town is playable" is true.
  "Coming soon" and "the best" are not allowed.
- Say it is built with AI agents under the maintainer's direction. Do not hide
  it and do not make it the headline.
- One link, the tagged one for that community. Attach pictures; do not hotlink.
- No donation link, no money talk ([support.md](../support.md): not open).
- Reply to comments as a person, from the [FAQ](faq.md). Do not argue. If a
  mod says no, say thanks and mark it declined in the [tracker](tracker.md).
- Run the text through the `unslop` skill if you change it.

## Order

1. Read the community's rules and pick the channel (file section "Where").
2. Send the "Ask the mods" message. Wait for a yes. Record "asked" in the tracker.
3. Post the "Post" text with the pictures. Record "posted", the date and the link.
4. Reply to people for a day or two. Record what came back.
5. The weekly summary shows whether it brought anyone.

## Tagged links

Every post carries one link: `https://life-framework.org/join/<source>`. The
source is the last path part, so Cloudflare Web Analytics lists each community
under Path (Web Analytics groups by path, referrer and country; it has no UTM
view, which is why the tag is a path). The page sends visitors to the Discord
and the playtest page.

| Source | Link |
| --- | --- |
| arma-reforger-discord | https://life-framework.org/join/arma-reforger-discord |
| everon-life | https://life-framework.org/join/everon-life |
| overthrow | https://life-framework.org/join/overthrow |
| reddit-armareforger | https://life-framework.org/join/reddit-armareforger |
| github | https://life-framework.org/join/github |

A new community: add it to `website/content/outreach-sources.json` first, then
a file here, a [tracker](tracker.md) row, and the same name in the link.

**Joins by source.** In our Discord, create one invite per source (Server
Settings, Invites; name it after the source, no expiry, no limit). Discord
counts each invite's uses. Paste the invite into `invite` for that source in
`outreach-sources.json`; the page then sends its visitors to it. Until then
visitors get the plain invite and the weekly summary shows visits only.

## The files

| Community | File | Posting style |
| --- | --- | --- |
| Official Arma Reforger Discord | [arma-reforger-discord.md](arma-reforger-discord.md) | Short, with 2 pictures; mods' permission first |
| Everon Life | [everon-life.md](everon-life.md) | Respectful and short; we started from their code |
| Overthrow | [overthrow.md](overthrow.md) | Credit first, then the differences |
| r/ArmaReforger | [reddit-armareforger.md](reddit-armareforger.md) | Title and body, 3 pictures; the monthly thread if mods prefer |
| Anywhere else | [template.md](template.md) | Copy it, fill every section |

## Pictures

The set is in `website/public/outreach/` (on the site at
`https://life-framework.org/outreach/`), with captions, alt text and a credit
line in its [index](../../../website/public/outreach/README.md): 14 screenshots
in 16:9 and 9:16, and 4 slideshow GIFs (stills in sequence, not footage). The
files below name pictures from it.

Kit v1 is text and pictures so posting can start before the hero video (#571)
exists. When the video lands, add it to each file's "Attach" line.

Real motion clips still to capture (a game session each) are listed in the
index under "Clips still to capture".

## Other places this feeds

- The landing page `https://life-framework.org/join` (#568).
- Our Discord's welcome and FAQ pins ([onboarding](../onboarding.md)).
- The playtest sessions ([playbook](../../playtest/session-playbook.md)).
