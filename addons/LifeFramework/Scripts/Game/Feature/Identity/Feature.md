# Identity

## Intent

A registered resident carries a photo identity card: a real item with their face, name, age,
date of issue, card number and the registry stamp. It can be shown, asked for by the police,
lost or stolen, and replaced at the town clerk for a fee.

## Interaction pattern

In-world first: the card is a held item, shown face to face (Show ID in the player interaction
menu), and replaced by talking to the town clerk (a reply in the clerk's conversation). A sheet
is needed only to read the card, because the portrait and five printed lines do not read from a
3D card in the hand. The sheet is the card itself, built from UI kit parts with a `PORTRAIT`.

## Photo id card (#902)

Design. The server owns every card ever issued in `LF_PhotoIdCard`: one `LF_PhotoIdRow` per
card (number, holder character id, first and last name, age, in-game issue date, the holder's
look as `LF_WantedLook` GUIDs). The card item (`IDCard.et`) carries only its number, in an
`LF_SerialComponent`, which already persists with the item (carried snapshot `itemserials`,
stashes, the ground); `LF_IdCardComponent` keeps the printed holder and name as RplProps, as
before. The town clerk issues the first card (arrival chain, `LF_QuestRewards.HandIdCard`) and
the look is captured then, from the body at the clerk's desk. A replacement is a new row with a
new number, a new photo and today's date; the old number is then void, and a void card shows
VOID on the sheet. Spawning no longer hands out a fresh card every time: it prints a carried
card that came back blank from a restart (`LF_IdCard.RepairCarried`), gives a first card to a
resident who earned one before this change and has no row, and, only in a mode with
`m_bGrantIdAtSpawn`, gives a free replacement. Everyone else who lost theirs goes to the clerk.

- **Player sees:** in the inventory, Inspect on the card opens the card sheet (portrait, name,
  age, issued, number, stamp). Show ID opens the same sheet on the other player's screen for
  `LF_IdCardView.SHOW_SECONDS`. An on-duty officer gets *Request ID* in the interaction menu;
  the citizen is asked on a small sheet and either shows the card or refuses. At the clerk, *I
  have lost my identity card* prints a new card for the fee in carried cash.
- **Owner and side:** server decides everything (who carries which card, the fee, the request);
  `LF_IdCard.SendView` reads the row and sends the printed fields and the look to the viewer's
  machine. The client draws the portrait (`LF_WantedPortrait` in the UI kit `PORTRAIT` part)
  and deletes its dummy when the sheet closes. The client never reads the registry.
- **Data:** `Configs/Identity/LF_PhotoIdCard.conf` (`LF_PhotoIdCardConfig`, owner section
  `idCard`): `replacementFee` (50). Persisted: the `idcards` payload (save version 30,
  `LF_PhotoIdRow`); the card number rides the existing `itemserials` payload with the item.
  A version 29 save has no rows: a resident who earned a card gets one printed at their next
  spawn (a blank carried one is printed, else a new one is handed over).
- **Proof:** `identity/photo-id-card` (LOGIC), `identity/photo-id-card-world` (WORLD),
  `persist/photo-id-cards` (PERSISTENCE), `node agent-box/session.mjs photo-id-card-proof`.
