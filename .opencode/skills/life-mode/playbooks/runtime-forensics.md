# Runtime forensics

Diagnose a live symptom (leak, idle spin, persistence corruption, a UI that freezes, an economy that drifts) from logs and instrumentation. The deliverable is a diagnosis, not a fix. A wrong fix is worse than no fix.

1. Capture the symptom. Read the dedicated server logs (`console.log`, `error.log` under `Documents/My Games/ArmaReforger/logs/`) and the Workbench script log for the affected window. Note the timeline: when it started, what preceded it, whether it is periodic.
2. Establish what is normal. Compare the failing window against a healthy baseline. This mod has healthy baselines: `tools\cli test` boot smoke on the DebugWorld, a known-good server run. If you cannot say what "normal" looks like, you cannot diagnose drift.
3. Form hypotheses from the mechanism, not the symptom. Script-side work (loops over inventory, per-frame timers, RPC storms, manager singletons that grow arrays) is the usual suspect in a Life mod. Enfusion systems to check first: replication frequency, `EOnFrame` work, per-entity timers, persistence write churn.
4. Instrument and confirm. Add logging or an `EL_Test` probe that reads the actual state (array length, tick count, memory-relevant counters) and run it. Eliminate hypotheses until one survives with runtime evidence per **principle-fix-root-causes**. Never ship the fix from a hypothesis.
5. Separate the diagnosis from the remedy. Deliver the mechanism, the evidence, and the trigger. If the user asked for a fix, propose it as a follow-up through the Bug fix playbook, sized by the evidence.

**Reply:** the symptom timeline, the normal baseline, the surviving mechanism, the evidence for it, and the diagnostic trail left behind for the fix.