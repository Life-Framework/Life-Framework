# Prototype

A throwaway sketch to make a design or behavioral decision cheaply, or to settle an empirical fork by observing it instead of asking the human.

1. Name the fork. What exactly is being decided, and what would each outcome mean for the design? Write both options as concrete predictions: "if we do X, the woodcutting action will feel responsive" is a claim you can test, not a preference.
2. Build the cheapest sketch that discriminates. Prefer an `EL_Test` that measures a value, a minimal scripted component dropped on a DebugWorld entity, or a scratch prefab. Do not build production structure. One prototype per competing option when the fork is genuinely open.
3. Observe, measure, decide. Drive the sketch in the DebugWorld or read the test output. Record the outcome next to each option. Let the result decide per **principle-never-block-on-the-human**. The prototype is the experiment; the human reacts to the result, they do not pre-decide it.
4. Settle the fork, then delete the prototype. A prototype that survives as production code is how mods accumulate dead weight. Extract only the decision, never the sketch. If the prototype unexpectedly becomes the feature, rebuild it cleanly through the Feature playbook.
5. When the fork is about two competing implementations rather than a measurement, build both and compare side by side. Two sketches are cheaper than one wrong architecture.

**Reply:** the fork, the sketch, the observed outcome, and the decision it settled. No prototype code remains.