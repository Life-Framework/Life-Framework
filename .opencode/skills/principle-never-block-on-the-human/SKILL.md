---
name: principle-never-block-on-the-human
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are tempted to ask the user a 'should I do X' question about reversible Enfusion work. Proceed, present the result, let the human course-correct after the fact. Never auto-load for routine work."
---

# Never Block on the Human

Proceed, present the result, let the human course-correct after the fact; reserve confirmation for irreversible actions.

**Why:** Every question you stop to ask costs the human a round trip. Most decisions in mod work are reversible: a component value, a config number, a renamed file. The human is better served by a result they can react to than a decision they must make blind.

**Rule:**
- Reversible work proceeds without asking. Change the value, write the code, move the file, then present it with the reasoning.
- The exception is a genuine product or preference call no experiment can settle (a game-feel choice, an economy balance, a monetization decision). Even then, prefer offering a recommended default plus the result over an open question.
- Reserve confirmation for irreversible actions: force-push, deleting resource GUIDs, publishing to Workshop, touching player or server data.
- When the answer is an observable fact, never ask: observe it. Build the prototype, read the base game, run the test.

**In this repo:**
- Economy and job balance values are reversible config data. Pick a sane default, ship it, let the server owner tune it. That is the framework's whole design.
- A prefab change is reversible until it is baked into a saved world. Saved-world references are the boundary where a change becomes expensive, so check **enfusion-blast-radius** before moving or deleting resources.
- Present work in the state the human can most quickly react to: the diff, the in-game result, the decision trail. Not the intermediate deliberation.