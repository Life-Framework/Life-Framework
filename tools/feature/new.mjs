// Feature scaffold: tools\cli feature new <Area> <Name> [--kind logic|world|ui|item] [--issue N]
//
// Writes the files every feature PR used to re-create by hand, in the shapes the
// validators expect, so an agent starts from a skeleton that compiles and already
// fails the right checks until the real work is done:
//
//   Feature/<Area>/LF_<Name>.c          the rule, one logging path, ResetStatics
//   Feature/<Area>/LF_<Name>Action.c    world, item: the F-key action
//   Feature/<Area>/LF_<Name>Sheet.c     ui: a paper sheet built from UI kit parts
//   Feature/<Area>/Feature.md           created, or a section appended
//   Tests/LF_Test_<Name>.c              LOGIC, green, RedProof() hook in place, red-proof PENDING (validate-tests refuses it)
//   Tests/LF_Test_<Name>World.c         world, ui, item: WORLD, fails until written
//   agent-box/proofs/<name>-proof.mjs   world, ui, item: the session proof and its shots
//   Language/everonlife_localization.st rows appended (then regen-localization)
//   Core/LF_StaticReset.c               the ResetStatics call
//   docs/features.md, docs/playtest.md  the row and the "what to try" line
//   tmp/pr-body-<slug>.md               the PR body `wt ship` checks (tools/lib/pr-quality.mjs)
//   tmp/feature-<name>.md               the ordered checklist for this kind
//
// Every placeholder is marked TODO(scaffold); `wt ship` refuses a PR body that still
// has one, and `tools\cli feature check` lists the ones left in the tree.
// The selftest (tools/validation/feature-scaffold-selftest.mjs) runs it on a scratch tree.

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

export const KINDS = ["logic", "world", "ui", "item"];
export const MARK = "TODO(scaffold)";
const ADDON = "addons/LifeFramework";
const GAME = `${ADDON}/Scripts/Game`;
const ST = `${ADDON}/Language/everonlife_localization.st`;
const STATIC_RESET = `${GAME}/Core/LF_StaticReset.c`;
const CLI = join(dirname(fileURLToPath(import.meta.url)), "..", "cli.mjs");

const USAGE = `usage: tools\\cli feature new <Area> <Name> [--kind logic|world|ui|item] [--issue N] [--dry-run]
       tools\\cli feature check            list the ${MARK} markers left in this worktree

  <Area>   the folder under Scripts/Game/Feature (Police, Signs, ...); created when new
  <Name>   PascalCase, becomes LF_<Name> (SpikeStrip, CreditsBoard)
  --kind   logic  a rule with no world object (default)
           world  something a player walks up to: adds the action, a WORLD test, a session proof
           ui     a sheet a player reads or fills in: adds a UI kit screen as well
           item   something a player carries: world, plus the inventory icon and preview checks`;

function guid() {
  return randomBytes(8).toString("hex").toUpperCase();
}

export function identity(area, name, kind = "logic", issue = null) {
  if (!/^[A-Z][A-Za-z0-9]*$/.test(area ?? "")) return { error: `<Area> must be PascalCase, got '${area ?? ""}'` };
  if (!/^[A-Z][A-Za-z0-9]*$/.test(name ?? "")) return { error: `<Name> must be PascalCase, got '${name ?? ""}'` };
  if (/^(LF|EL)$/i.test(name) || /^(LF|EL)_/i.test(name)) return { error: "<Name> comes without the LF_ prefix" };
  if (!KINDS.includes(kind)) return { error: `--kind must be one of ${KINDS.join(", ")}` };
  if (issue !== null && !/^\d+$/.test(String(issue))) return { error: `--issue takes a number, got '${issue}'` };
  const words = name.replace(/([a-z0-9])([A-Z])/g, "$1 $2").split(" ");
  const kebab = words.map((w) => w.toLowerCase()).join("-");
  const title = words.map((w, i) => (i === 0 ? w : w.toLowerCase())).join(" ");
  return {
    area,
    name,
    kind,
    issue: issue === null ? null : Number(issue),
    cls: `LF_${name}`,
    kebab,
    title,
    testName: `${area.toLowerCase()}/${kebab}`,
    ref: issue === null ? "" : ` (#${issue})`,
    visible: kind !== "logic",
  };
}

// ---------------------------------------------------------------- templates

const RULE = "\t//------------------------------------------------------------------------------------------------";

function scriptFile(id) {
  return `// LF_${id.name}.c — ${id.title}${id.ref}.
// ${MARK}: one line on what this does, who calls it and on which side (server or client).

class ${id.cls}
{
	protected static int s_iUses;

${RULE}
	//! The rule, decided on the server. ${MARK}: replace the parameter with the real inputs
	//! (account, faction, distance, cooldown) and log every refusal with its reason.
	static bool CanUse(bool hasCharacter)
	{
		if (!hasCharacter)
		{
			LF_Debug.Warn("${id.area}", "${id.kebab} refused: no character");
			return false;
		}

		return true;
	}

${RULE}
	//! Server. Returns false and logs when refused; a silent no-op is a bug.
	static bool Use(bool hasCharacter)
	{
		if (!CanUse(hasCharacter))
			return false;

		s_iUses = s_iUses + 1;
		LF_Debug.Info("${id.area}", string.Format("${id.kebab} used (%1)", s_iUses));
		return true;
	}

${RULE}
	static int GetUses()
	{
		return s_iUses;
	}

${RULE}
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_iUses = 0;
	}
}
`;
}

function actionFile(id) {
  return `// LF_${id.name}Action.c — the F-key action for ${id.title.toLowerCase()}${id.ref}.
// ${MARK}: add this action to the prefab's ActionsManagerComponent, then run
// \`node tools/actions/place-action-contexts.mjs\` so it has a context and a reachable anchor.

class ${id.cls}Action : ScriptedUserAction
{
${RULE}
	override bool CanBeShownScript(IEntity user)
	{
		return user != null;
	}

${RULE}
	override bool CanBePerformedScript(IEntity user)
	{
		if (!${id.cls}.CanUse(user != null))
		{
			SetCannotPerformReason("#LF-${id.name}_Denied");
			return false;
		}

		return true;
	}

${RULE}
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-${id.name}_Action";
		return true;
	}

${RULE}
	//! Runs on the server (no HasLocalEffectOnlyScript): the client asks, the server decides.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error("${id.area}", "${id.kebab} action performed with no owner or user");
			return;
		}

		${id.cls}.Use(true);
	}
}
`;
}

function sheetFile(id) {
  return `// LF_${id.name}Sheet.c — the paper sheet for ${id.title.toLowerCase()}${id.ref}. Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 theme from LF_UITheme: no new
// .layout, no colours or fonts here (docs/style-guide.md "Screens").
// ${MARK}: list the real parts; every text is a #key in the string table.

class ${id.cls}Sheet
{
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_UIScreen s_Screen;

${RULE}
	static array<ref LF_UIPart> GetParts()
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", "#LF-${id.name}_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Line", "#LF-${id.name}_Action"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}

${RULE}
	//! \\param anchor the world object the sheet belongs to; walking away closes it.
	static LF_UIScreen Open(EntityID anchor)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(GetParts());
		if (!s_Screen)
		{
			LF_Debug.Error("${id.area}", "${id.kebab} sheet failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (anchor != EntityID.INVALID)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor);

		LF_Debug.Info("${id.area}", "${id.kebab} sheet open");
		return s_Screen;
	}

${RULE}
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

${RULE}
	static void Hide()
	{
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}
}
`;
}

function logicTest(id) {
  return `// red-proof: PENDING ${MARK}: run \`tools\\cli test --no-build --red LF_Test_${id.name}\` (batch every new test of the change into one --red run), paste the observed [LFTEST] RED-OK line here. The RedProof() hook below skips the act; no hand edit, no restore.
// tier: LOGIC
//! ${id.title}${id.ref}: the rule, with no world.
class LF_Test_${id.name} : LF_Test
{
	override string GetName()
	{
		return "${id.testName}";
	}

	override void Run(LF_TestContext ctx)
	{
		${id.cls}.ResetStatics();

		ctx.False(${id.cls}.Use(false), "no character is refused");
		ctx.True(${id.cls}.GetUses() == 0, "a refused use changes nothing");
		// Red-proof hook: with --red the act is skipped, so the next two assertions must fail.
		// Keep the hook on the act that the assertions below depend on.
		bool allowed = false;
		if (!RedProof())
			allowed = ${id.cls}.Use(true);

		ctx.True(allowed, "a character is allowed");
		ctx.True(${id.cls}.GetUses() == 1, "an allowed use is counted once");

		${id.cls}.ResetStatics();
		ctx.True(${id.cls}.GetUses() == 0, "the static reset clears the count");
	}
}
`;
}

function worldTest(id) {
  return `// red-proof: PENDING ${MARK}: this test fails as generated. Once the real assertion is written, add the RedProof() hook (skip the act: if (!RedProof()) ...), list this test in the change's one --red run and record the observed [LFTEST] RED-OK line here.
// tier: WORLD
//! ${id.title}${id.ref}: the placed thing exists and offers what a player expects.
class LF_Test_${id.name}World : LF_Test
{
	override string GetName()
	{
		return "${id.testName}-world";
	}

	override void Run(LF_TestContext ctx)
	{
		// ${MARK}: load the prefab (or find the placed entity by name) and assert what a player
		// meets: the action is offered, the text keys resolve, a broken input logs LF_Debug.Error.
		ctx.True(false, "${id.testName}-world is not written yet");
	}
}
`;
}

function proofFile(id) {
  const verb = `${id.kebab}-proof`;
  const fn = `run${id.name}Proof`;
  return `import { arrive, clientBox, sessionDir } from "../session.mjs";

// ${id.title}${id.ref} on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full     (one client; add --clients 2 only for a second player)
//   node agent-box/session.mjs ${verb}
//   node agent-box/session.mjs down
// ${MARK}: name the placed entity and the action, judge the result from \`state\` or \`seen\`,
// and keep the two shots: they are the PR's "after" pictures (tools\\cli evidence).
const SITE = "${MARK}_EntityName";
const ACTION = "${id.cls}Action";

async function ${fn}() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(\`client \${n}: \${r.line.replace(/^\\d+ /, "")} -> \${r.status} \${r.detail.slice(0, 200)}\`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(\`\${what} returned \${r.status} \${r.detail}\`);
    return r;
  };

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: SITE }), "stand at the site");
  wait(2500);
  const look = must(on(1, { verb: "look" }), "look");
  if (!look.detail.includes(ACTION)) fails.push(\`the F key does not offer \${ACTION} at \${SITE}: \${look.detail.slice(0, 200)}\`);
  must(on(1, { verb: "use", className: ACTION, entityName: SITE }, 10000), "use the action");
  wait(1500);

  if (fails.length) {
    console.log(\`${verb}: FAIL\`);
    for (const f of fails) console.log(\`  \${f}\`);
    return 1;
  }
  console.log("${verb}: OK");
  return 0;
}

export default { name: "${verb}", usage: "${verb}", run: () => ${fn}() };
`;
}

function featureMdNew(id) {
  return `# ${id.area}

## Intent

${MARK}: what a player gets from this area, in two sentences.

## Interaction pattern

${MARK}: in-world first (docs/decisions.md). What object does the player walk up to, what does the F key offer, and what, if anything, opens a sheet?

${featureMdSection(id)}`;
}

function featureMdSection(id) {
  return `## ${id.title}${id.ref}

- **Player sees:** ${MARK}
- **Owner and side:** \`${id.cls}\` decides on the server${id.kind === "ui" ? `; \`${id.cls}Sheet\` is client only` : ""}.
- **Data:** ${MARK}: which config section and which string keys drive it; what is persisted (nothing, or the payload name).
- **Proof:** \`${id.testName}\` (LOGIC)${id.visible ? `, \`${id.testName}-world\` (WORLD), \`node agent-box/session.mjs ${id.kebab}-proof\`` : ""}.
`;
}

function stRows(id) {
  const rows = [
    ["Action", id.title, `F-key action name for ${id.title.toLowerCase()}${id.ref}.`],
    ["Denied", `${id.title} is not available.`, `Greyed-out reason and toast when ${id.title.toLowerCase()} is refused${id.ref}.`],
  ];
  if (id.kind === "ui") rows.push(["Title", id.title.toUpperCase(), `Printed heading of the ${id.title.toLowerCase()} sheet${id.ref}.`]);
  return rows
    .map(([suffix, text, comment]) => [`    CustomStringTableItem "{${guid()}}" {`, `     Id "LF-${id.name}_${suffix}"`, `     Target_en_us "${text}"`, `     Comment "${comment}"`, "    }"].join("\n"))
    .join("\n");
}

function featuresRow(id) {
  const today = new Date().toISOString().slice(0, 10);
  return `
### \`${id.cls}\` — \`Feature/${id.area}/${id.cls}.c\`${id.issue ? ` (#${id.issue}, ${today})` : ` (${today})`}
- ⚠️ ${MARK}: what works, in one line a player would recognise. Tests: \`${id.testName}\` (LOGIC)${id.visible ? `, \`${id.testName}-world\` (WORLD); session proof \`${id.kebab}-proof\`` : ""}.
`;
}

function playtestLine(id) {
  return `- **${id.title}${id.ref}.** ${MARK}: where it is in the town (entity name and x, z), what to do, and what you should see. Demo: \`node agent-box/session.mjs ${id.kebab}-proof\`.`;
}

export function prBody(id) {
  const shots = id.visible
    ? `## Screenshots

${MARK}: before and after from the same angle. \`tools\\cli look <area>\` before the first edit and after the last,
then \`tools\\cli evidence ${id.issue ?? id.kebab} before|after <area>\` and paste the table \`tools\\cli evidence ${id.issue ?? id.kebab} table\` prints.

`
    : "";
  return `## Summary

${MARK}: what changed and why, two sentences. What a player notices.

**In-world first:** ${MARK}: the object the player walks up to, or why this needs a sheet (docs/decisions.md).

${shots}## Evidence

- Red: ${MARK}: the observed \`[LFTEST] FAIL\` line for each new test.
- Green: ${MARK}: \`SUMMARY tier=all ...\`${id.visible ? ` and the \`${id.kebab}-proof: OK\` line` : ""}.
${id.visible ? `- Polish checklist (\`.claude/skills/polish\`): ${MARK}: every line ticked, or the exception named.\n` : ""}
## Review

${MARK}: \`tmp/review-<slug>.md\` findings, each fixed or dismissed with the reason; or "Not reviewed: <why this diff is low risk>".

## Manual testing

**Steps**

1. ${MARK}: host or join, where to go, what to press.

**Expected**

${MARK}

🤖 Generated with [Claude Code](https://claude.com/claude-code)
`;
}

function checklist(id, slug) {
  const steps = [];
  const add = (head, ...lines) => steps.push(`## ${steps.length + 1}. ${head}\n\n${lines.map((l) => `- [ ] ${l}`).join("\n")}`);
  add("Design (before any code)", "Write the one-paragraph spec into `Feature.md`: who calls it, which side, what is data, what persists.", "In-world first: name the object the player walks up to. A sheet only where typing or reading detail needs it.", "Look up every unfamiliar API (`tools\\cli call api_search ...`); read one sibling feature.");
  if (id.visible) {
    add("Look before", "`tools\\cli look <area>`: read `contact.jpg`. This is the before picture and the scene the new thing must fit.", `\`tools\\cli evidence ${id.issue ?? id.kebab} before <area>\` keeps it for the PR.`);
    add("Place", "Pick the prop from `docs/style-guide.md` (period, material, where it belongs: boards on walls, desks indoors or under a roof).", "`tools\\cli place on|against|ground ...`; never typed coordinates.", "`tools\\cli audit placement --area <area> --strict` is clean.");
  }
  add("Build", `Fill \`${id.cls}.c\`${id.kind === "ui" ? ` and \`${id.cls}Sheet.c\` (UI kit parts only)` : ""}; every refusal logs through \`LF_Debug\`.`, "Strings are keys in the string table (rows already appended; `tools\\cli regen-localization` after editing).", "`tools\\cli validate` after each step.");
  add("Test (red first, observed)", `\`tools\\cli test --no-build --only LF_Test_${id.name}\` until green, then ONE \`tools\\cli test --no-build --red LF_Test_${id.name}[,...]\` for every new test of the change: the \`RedProof()\` hook (in the scaffold) must make each one FAIL. Copy the RED-OK line into \`// red-proof:\`. A bug fix needs no extra run: its fail-then-pass on the unfixed and fixed code is the proof.`, id.visible ? `Write \`LF_Test_${id.name}World\`; give it the same \`RedProof()\` hook and list it in the same \`--red\` run.` : "Add a WORLD test only if a prefab or the world is involved.", "`tools\\cli test --no-build --tier all` once, at the end.");
  if (id.visible) {
    add("Look after", "`tools\\cli look <area>` again; open the close-up of the new thing. Walk the polish checklist (`.claude/skills/polish/SKILL.md`).", `\`tools\\cli evidence ${id.issue ?? id.kebab} after <area>\`.`, id.kind === "ui" || id.kind === "item" ? "One `--profile full` session for what Workbench cannot show (the sheet, the held item, the inventory icon and preview): run the proof, keep its shots, `session down`." : "A session only if a second player, motion or UI is part of the claim.");
  }
  add("Review", "Risky diff (replication, persistence, RPC, money, GUIDs)? Run **interrogate**; the reviewer writes `tmp/review-" + slug + ".md`.", "Tick every finding there as fixed or dismissed with a reason. `wt ship` refuses open findings.");
  add("Ship", `Fill \`tmp/pr-body-${slug}.md\`; no ${MARK} left (\`tools\\cli feature check\`).`, `\`tools\\cli wt ship ${slug}${id.issue ? ` --refs ${id.issue}` : ""} --body-file tmp/pr-body-${slug}.md\``, 'Report "merged, in-dev" with the PR URL and the screenshots.');
  return `# ${id.title}: ${id.kind} feature checklist\n\nThe order is the playbook (\`.claude/skills/life-mode/playbooks/feature.md\`). Tick as you go.\n\n${steps.join("\n\n")}\n`;
}

// ------------------------------------------------------------------ writing

function insertBefore(text, marker, addition, what) {
  const at = text.lastIndexOf(marker);
  if (at === -1) throw new Error(`${what}: marker not found`);
  return text.slice(0, at) + addition + text.slice(at);
}

export function slugOf(root) {
  const m = /-ws-(.+)$/.exec(basename(root));
  return m ? m[1] : "feature";
}

// The plan: [{ path, body, mode: "create" | "edit" }]. Pure apart from reading the tree.
export function plan(root, id) {
  const out = [];
  const read = (rel) => readFileSync(join(root, rel), "utf8");
  const has = (rel) => existsSync(join(root, rel));
  const create = (rel, body) => {
    if (has(rel)) throw new Error(`${rel} already exists; pick another <Name> or extend it by hand`);
    out.push({ path: rel, body, mode: "create" });
  };
  const edit = (rel, body) => out.push({ path: rel, body, mode: "edit" });
  const slug = slugOf(root);
  const dir = `${GAME}/Feature/${id.area}`;

  create(`${dir}/${id.cls}.c`, scriptFile(id));
  if (id.kind === "world" || id.kind === "item") create(`${dir}/${id.cls}Action.c`, actionFile(id));
  if (id.kind === "ui") create(`${dir}/${id.cls}Sheet.c`, sheetFile(id));
  if (has(`${dir}/Feature.md`)) edit(`${dir}/Feature.md`, `${read(`${dir}/Feature.md`).replace(/\n*$/, "\n")}\n${featureMdSection(id)}`);
  else out.push({ path: `${dir}/Feature.md`, body: featureMdNew(id), mode: "create" });

  create(`${GAME}/Tests/LF_Test_${id.name}.c`, logicTest(id));
  if (id.visible) {
    create(`${GAME}/Tests/LF_Test_${id.name}World.c`, worldTest(id));
    create(`agent-box/proofs/${id.kebab}-proof.mjs`, proofFile(id));
  }

  const st = read(ST);
  if (st.includes(`"LF-${id.name}_Action"`)) throw new Error(`string table already has LF-${id.name}_Action`);
  edit(ST, insertBefore(st, "\n  }\n }\n}", `\n${stRows(id)}`, ST));

  if (has(STATIC_RESET)) {
    edit(STATIC_RESET, insertBefore(read(STATIC_RESET), "\n\n\t\ts_iRuns = s_iRuns + 1;", `\n\t\t${id.cls}.ResetStatics();`, STATIC_RESET));
  }

  edit("docs/features.md", `${read("docs/features.md").replace(/\n*$/, "\n")}${featuresRow(id)}`);
  if (id.visible && has("docs/playtest.md")) {
    edit("docs/playtest.md", insertBefore(read("docs/playtest.md"), "\n### Known rough edges", `${playtestLine(id)}\n`, "docs/playtest.md"));
  }

  out.push({ path: `tmp/pr-body-${slug}.md`, body: prBody(id), mode: "scratch" });
  out.push({ path: `tmp/feature-${id.kebab}.md`, body: checklist(id, slug), mode: "scratch" });
  return out;
}

export function apply(root, steps) {
  for (const s of steps) {
    const p = join(root, s.path);
    mkdirSync(dirname(p), { recursive: true });
    writeFileSync(p, s.body, "utf8");
  }
}

// Every TODO(scaffold) left in tracked source (not tmp/): the work the skeleton still asks for.
export function leftovers(root) {
  const hits = [];
  const walk = (rel) => {
    const abs = join(root, rel);
    if (!existsSync(abs)) return;
    for (const name of readdirSync(abs)) {
      const r = `${rel}/${name}`;
      const st = statSync(join(root, r));
      if (st.isDirectory()) walk(r);
      else if (/\.(c|md|mjs|st)$/.test(name) && st.size < 4_000_000) {
        const lines = readFileSync(join(root, r), "utf8").split("\n");
        lines.forEach((l, i) => {
          if (l.includes(MARK)) hits.push(`${r}:${i + 1}: ${l.trim().slice(0, 140)}`);
        });
      }
    }
  };
  for (const top of [`${GAME}/Feature`, `${GAME}/Tests`, "agent-box/proofs", `${ADDON}/Language`]) walk(top);
  for (const doc of ["docs/features.md", "docs/playtest.md"]) {
    if (!existsSync(join(root, doc))) continue;
    readFileSync(join(root, doc), "utf8").split("\n").forEach((l, i) => {
      if (l.includes(MARK)) hits.push(`${doc}:${i + 1}: ${l.trim().slice(0, 140)}`);
    });
  }
  return hits;
}

function arg(list, flag) {
  const i = list.indexOf(flag);
  return i === -1 ? null : (list[i + 1] ?? "");
}

export async function main(args, ctx = {}) {
  const list = args ?? [];
  const root = ctx.root ?? join(dirname(CLI), "..");
  const sub = list[0];
  if (sub === "check") {
    const hits = leftovers(root);
    if (!hits.length) {
      console.log(`feature check: OK (no ${MARK} left)`);
      return 0;
    }
    console.log(`feature check: ${hits.length} ${MARK} marker(s) left`);
    for (const h of hits) console.log(`  ${h}`);
    return 1;
  }
  if (sub !== "new") {
    console.log(USAGE);
    return sub ? 1 : 0;
  }
  const positional = [];
  for (let i = 1; i < list.length; i++) {
    if (list[i] === "--kind" || list[i] === "--issue") i++;
    else if (!list[i].startsWith("--")) positional.push(list[i]);
  }
  const id = identity(positional[0], positional[1], arg(list, "--kind") ?? "logic", arg(list, "--issue"));
  if (id.error || positional.length !== 2) {
    console.log(`feature new: ${id.error ?? "takes <Area> <Name>"}`);
    console.log(USAGE);
    return 1;
  }
  if (!ctx.scratch) {
    const { isMainCheckout } = await import("../wt.mjs");
    if (isMainCheckout(root)) {
      console.log("FAIL  feature new writes into the checkout; run it in a worktree (tools\\cli wt new <slug>)");
      return 1;
    }
  }
  let steps;
  try {
    steps = plan(root, id);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
  const dry = list.includes("--dry-run");
  console.log(`feature new: ${id.cls} (${id.kind}) in Feature/${id.area}${dry ? " [dry run]" : ""}`);
  for (const s of steps) console.log(`  ${s.mode.padEnd(7)} ${s.path}`);
  if (dry) return 0;
  apply(root, steps);
  if (!ctx.scratch) {
    for (const regen of ["regen-localization", "regen-tests"]) {
      const r = spawnSync(process.execPath, [CLI, regen], { cwd: root, windowsHide: true, encoding: "utf8" });
      console.log(`  ${regen}: ${r.status === 0 ? "ok" : `FAILED\n${(r.stdout + r.stderr).trim()}`}`);
      if (r.status !== 0) return 1;
    }
  }
  console.log(`next: open ${relative(root, join(root, "tmp", `feature-${id.kebab}.md`)).replace(/\\/g, "/")} and work down it. ${leftovers(root).length} ${MARK} marker(s) to replace.`);
  return 0;
}
