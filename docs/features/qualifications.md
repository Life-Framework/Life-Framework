# Qualifications

### `LF_QualificationRules`, `LF_QualificationConfig` — `Feature/Qualifications/`, `Configs/Qualifications/LF_Qualifications.conf` (#3424, 2026-11-19)
- A course (id, faction, minimum rank, fee, study time, unlock ids) is config, and a pure rule says whether a character may take it: the wrong faction, too low a rank, a course already held and a short purse are each refused with their own localization key (`LF-Qual_*`), and a pass returns the unlock ids (strings such as `detective`, `traffic`), the fee and the study time. The core ships four neutral courses; a pack adds, replaces, patches or removes courses by id through an overlay. Nothing takes money, runs the study or saves a held course yet. Tests: `qualifications/rules` (LOGIC).
