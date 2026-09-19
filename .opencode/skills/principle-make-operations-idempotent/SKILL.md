---
name: principle-make-operations-idempotent
description: "Use for persistence, save/load, respawn, reconnect, retry, or repeated update paths. Make repeated execution converge to one valid state."
---
# Idempotence
Design retries, reloads, reconnects, and repeated ticks to converge rather than duplicate work. Persist stable keys, version payloads, tolerate missing objects, and make load safe on both fresh and live state.
