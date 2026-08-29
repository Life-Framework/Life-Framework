# Session pickup

Resume or take over a prior agent's in-flight work. The goal is to reconstruct current state from artifacts, not from memory.

1. Read the working tree. `git status`, `git log --oneline -15`, and the uncommitted diff. Uncommitted work is the most recent state. Commit messages tell you where the previous session stopped.
2. Read the decision trail if one exists. Check for a show-me-your-work log, a NOTES file, or TODO markers. The **show-me-your-work** skill writes a TSV the previous session may have left.
3. Read the todo list state and any phase plan. Match them against the actual tree. Trust the tree over the notes: notes describe intent, the diff describes reality per **principle-prove-it-works**.
4. Establish the verification state. What was proven in-game and what was not. Run `tools\cli validate` for a quick health check and, if the last change touched scripts, boot the DebugWorld or run the boot smoke before continuing.
5. Re-scope with the human only if the trail is ambiguous. Otherwise continue the work per **principle-never-block-on-the-human**: pick up the first unverified unit and drive it to a verified state.
6. When you complete the takeover, write a one-paragraph current-state brief for the next session: what is done, what is proven, what is next.

**Reply:** the reconstructed current state, what is proven and what is not, and the first unit you will verify or continue.