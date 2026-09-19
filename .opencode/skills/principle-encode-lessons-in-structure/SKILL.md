---
name: principle-encode-lessons-in-structure
description: "Use when the same instruction or bug fix appears twice. Prefer a validation, lint, test, or metadata contract over more prose."
---
# Encode Lessons
If a rule matters repeatedly, make `tools/validation`, `tools/lint`, `tools/test`, or an `EL_Test` enforce it. Checks should fail clearly and explain the fix. Prefer compile-time or pre-commit enforcement over comments.
