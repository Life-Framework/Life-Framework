# Chat — Design Requirements

> Context file for communication. Written against
> `docs/design-philosophy.md`.

## Intent

Chat is communication, and communication is inherently UI — this is the one
feature where a UI surface is the correct answer. There is no physical analog
for global chat, and we are not building one.

## Interaction pattern

Rung 4, accepted. Channels exist for roleplay and admin
(`AdminChatChannel`); `EL_ChatManager` routes them.

## V1 (shippable)

1. Fix the hard-coded Spanish prefixes and non-localized strings (AGENTS.md
   localization contract).
2. Keep admin chat gated to whitelisted players (see `Whitelist`).

## Iteration path

- **V2** — radio/proximity voice-style channel separation, RP prompts
  (`/me`, `/do`).

## Current state

- `EL_ChatManager` (`Feature/Chat/`) — hides chat until a player opens it.
- `AdminChatChannel` — admin-only channel, including `!wadd`.
- `LF_RoleplayChat` — OOC (`OOCGlobal`, prefix `/ooc`), OOC Local (`/oocl`),
  Twatter global, and admin styling (`/admin`). VoN display names stay hidden.
  ArmaVision (editor photo mode) is administrator-only.
- UI: `Assets/UI/Chat/`, `Configs/Chat/`, `UI/Chat/EL_ChatPanel`.

## Dependencies

- `Whitelist` (admin channel gating).