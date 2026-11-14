# Rename the framework's terms

The mod's own text says "police", "medic", "town" and "$". You can rename them for your server, in every
language the mod ships, without copying the string table.

1. List the terms: `tools\cli terms`. Each has forms (`singular`, `plural`, sometimes more) and the keys that use it.
2. Add a `terms` section to `config.json` (`$profile:LifeFramework/config.json`):

```json
"terms": {
  "police": "Sheriff",
  "town": { "en_us": "village", "de_de": "Dorf" },
  "medic": { "singular": "paramedic", "plural": "paramedics" },
  "currency": { "sym": "£" }
}
```

- Plain text is the singular **and** the plural, in every language.
- An object of forms (`singular`, `plural`, `sym`) sets only those forms.
- An object keyed by language (`en_us`, `de_de`, `fr_fr`, `it_it`, `es_es`, `pt_pt`, `pt_br`) sets that
  language. A language that has no entry keeps the shipped words.
- Restart, or run `/configreload`. Clients get the words from the server.

A pack ships its own words: a `LF_TermsConfig` overlay (see `addons/LifeFramework/Configs/Tests/LF_Test_TermsOverlay.conf`)
whose forms are string keys in the pack's table, so every language is translated. The owner's text wins over the pack's.

Terms: `police`, `medic`, `town`, `currency`, `org` (an organization: `"org": "Gang"`), `fine`, `job`.

Limits today: voiced lines keep the recorded word, and item and prop names, quest text, license and job names and
the spawn menu are translated by the engine, so they keep the shipped words (`docs/` Terms `Feature.md` lists them).
Action prompts, toasts, sheets and dialogue follow your words.
