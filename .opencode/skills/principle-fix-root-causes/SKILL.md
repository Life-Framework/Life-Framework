---
name: principle-fix-root-causes
description: "Use while debugging. Reproduce the symptom, trace the mechanism, and fix the cause instead of adding guards that hide it."
---
# Fix Root Causes
Reproduce from the actual log or runtime path. Trace the first incorrect state to its cause, fix that mechanism, and rerun the proof. Do not silence a crash or no-op with a defensive guard unless invalid input is genuinely expected at that boundary.
