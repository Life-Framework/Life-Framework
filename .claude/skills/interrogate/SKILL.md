---
name: interrogate
description: "Use before shipping a risky or contested Life Framework diff. Find real failures in replication, boundaries, resources, persistence, input, layouts, and GUIDs, then fix and verify them."
---

# Interrogate

Run it when the diff touches replication or an RPC, persistence, money or inventory, a base
prefab, or resource GUIDs. It found a real defect in five of seven recent feature PRs
(a misaligned `RplLoad`, an unchecked slot id from a client, a ring that a late joiner never
got, an unbounded string, a damaged rank). Skip it for routine, low-risk edits and say so in the
PR's `## Review` section.

1. Read the diff and state the behavior it claims to change.
2. Spawn a read-only reviewer subagent briefed with `.opencode/agent/el-reviewer.md`
   (opencode runs it as `el-reviewer`) and ask for defects, not style notes. Give it the diff
   and the claim, not your reasoning.
3. Run one independent adversarial pass focused on a different failure mode.
4. **Write every finding to `tmp/review-<slug>.md` in the worktree**, one per line, as an open
   tick box with the file and line:

   ```
   - [ ] LF_PhoneCalls.c:212 RpcAsk_Dial takes an unbounded string from the client
   ```

   A reviewer subagent cannot write there: copy its findings in yourself, all of them, before
   you judge any. A finding that lives only in a subagent's reply is lost at the next summary.
5. Resolve each line and tick it. `wt ship` refuses the PR while a line is open or a ticked
   line has no outcome:

   ```
   - [x] LF_PhoneCalls.c:212 unbounded string. fixed: capped at 64 in RpcAsk_Dial, test phone/dial-bounds
   - [x] LF_PhoneCalls.c:90 ring may miss a late joiner. dismissed: RplLoad writes the ring state (line 301), proven by phone-proof step 6
   ```

6. Re-run the relevant verification gate before shipping. `wt ship` appends the file to the PR
   body under `## Review findings`, so the maintainer sees what was found and what was dismissed.

The final report lists findings, dismissals, fixes, and evidence.
