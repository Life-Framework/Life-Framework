# Ops

> Server operations for the host: scheduled restarts and their warnings. M4, parent #1006.

## Today

`LF_RestartConfig` (#1343) is the `restarts` owner section and nothing else: `times` (daily
`HH:MM`, empty = no scheduled restarts) and `warningMinutes` (default 15, 5, 1). The scheduler and the
warnings (O5) read it; the field names are in the class comment.

Test: `config/m4-sections` (`LF_Test_M4ConfigSections`), which also covers the `staff` and `dayCycle`
sections declared in `Feature/Moderation/` and `Feature/Atmosphere/`.
