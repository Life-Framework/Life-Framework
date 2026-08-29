---
description: Run the verification ladder on the current change and report the evidence. Applies the enfusion-verify skill: repo hygiene, compile, test suite, in-game proof, and the restart / dedicated-server / late-joiner proofs where they apply.
---

Apply the `enfusion-verify` skill to the current state of this mod. Climb the rungs your change's scope requires: `tools\cli validate`, then `tools\cli test` where the change touches scripts or resources, then the in-game proof for the feature path. Read the logs, do not trust memory. For persistence or networked changes, run the restart, dedicated-server, and late-joiner proofs. Report each rung's evidence verbatim, and what remains unverified.

$ARGUMENTS