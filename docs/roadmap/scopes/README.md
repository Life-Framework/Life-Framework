# Scopes

A scope is one plan a contributor can finish without boiling the framework. It names the player loop, what ships in the first slice, which assets are legal, and the proof.

## Required headings

```
# Name

Phase: Alpha | Release | After release
Extends: the systems already in the tree this builds on

## Player loop
## In scope
## Out of scope
## Assets
## First slice
## Depends on
## Proof
```

Prefab and script names for each scope are in [sources.md](../sources.md). The headless case, DebugWorld entity, and screenshot for each scope are in [testing](../testing/README.md).

## Rules

- One loop per file. Bank, jail, and hospital share [forced entry](crime/forced-entry.md); they do not each invent a charge.
- Assets are base-game prefabs, chosen in config. A missing prefab logs an error and that site stays closed.
- The server decides the payout, the sentence, and the lock state. The client only asks.
- Player-facing strings are localization keys.
- Update [features.md](../../features.md) in the same change that implements the scope.

## Areas

| Folder | Covers |
| --- | --- |
| [crime/](crime/forced-entry.md) | Breach, robberies, contraband |
| [police/](police/jailing.md) | Sentence and ranks |
| [work/](work/resources.md) | Legal resources and civilian jobs |
| [ownership/](ownership/organizations.md) | Groups, hideouts, vehicles |
| [later/](later/housing.md) | After the release cut |
