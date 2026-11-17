# Property — Design Requirements

## Intent

Owned plots and houses built in stages. This slice (#2647, P2 of #747) is the data only:
`Configs/Property/LF_Property.conf` read through `LF_PropertyConfig`. Plots, tiers, four stages per tier,
per-stage item requirements, work units and shell prefabs are all config. Nothing places or builds yet.

## Rules

- A broken or missing config logs `LF_Debug.Error("Property", ...)` and the getters return 0, empty or null.
- The core holds generic example plots only; a pack overrides the file with its own plots and prices (#808).
- No rent in wave 1: `m_iRentIntervalHours` and `m_iGraceHours` of 0 mean owned outright.
