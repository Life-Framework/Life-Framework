// `tools\cli promote`: the human step between "merged to main" and "released".
// Logic lives in lib/release-gates.mjs; docs/worktrees.md "Release gates".
import * as wt from "./wt.mjs";
import * as rg from "./lib/release-gates.mjs";
import { evaluateQuorum, loadQuorum, readConfirmations } from "./lib/quorum.mjs";

function val(args, name) {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
}

export function promoteHelp() {
  console.log(`promote: move 'stable' to a commit a human verified on main

  tools\\cli promote [--to <sha>]                      list PRs merged since stable, write tmp/promote/checklist-<sha>.md
  tools\\cli promote [--to <sha>] --confirm             fast-forward stable to <sha> (default origin/main), tag it,
                                                       mark the issues of those PRs 'verified' and close them
  tools\\cli promote --to <sha> --confirm --fail N[,N]  those issues get 'regression' and reopen (refused when their
                                                       PR is inside the range: revert or fix on main, or pick an earlier --to)
  tools\\cli promote --fail N[,N]                       mark issues 'regression' without moving stable
  tools\\cli promote --revert <PR>                      open a PR that reverts one merged PR on main (before promotion)
  tools\\cli promote --init [--to <sha>]                create origin/stable once (default origin/main)

stable only moves forward (fast-forward). Never edit it directly; agents keep branching from main.`);
}

export async function cmdPromote(root, args, opts = {}) {
  if (args.includes("--help") || args.includes("-h")) {
    promoteHelp();
    return 0;
  }
  const log = (s) => console.log("  " + s);
  try {
    const to = val(args, "--to");
    if (to) rg.checkRefName(to);
    rg.fetchAll(root);

    if (args.includes("--init")) {
      const sha = rg.initStable(root, to);
      console.log(`created origin/stable at ${sha.slice(0, 8)}`);
      return 0;
    }

    const fail = (val(args, "--fail") ?? "").split(",").filter(Boolean).map(Number);
    if (fail.some((n) => !Number.isInteger(n) || n <= 0)) throw new Error("--fail takes issue numbers, e.g. --fail 12,14");
    const gh = opts.gh ?? rg.realGh(root);

    const revert = val(args, "--revert");
    if (revert !== null) return await revertCmd(root, gh, Number(revert));

    if (fail.length && !args.includes("--confirm")) {
      for (const n of fail) await rg.markRegression(gh, n, log);
      return 0;
    }

    const plan = rg.promotionPlan(root, { to });
    console.log(`stable ${plan.stable.slice(0, 8)} -> ${plan.target.slice(0, 8)}: ${plan.prs.length} PR(s) merged` + (plan.direct.length ? `, ${plan.direct.length} direct commit(s)` : ""));
    for (const c of plan.commits) console.log(`  ${c.pr ? "#" + c.pr : c.sha.slice(0, 8)}  ${c.subject}`);

    if (!args.includes("--confirm")) {
      if (plan.commits.length === 0) {
        console.log("nothing to promote: stable already contains the target");
        return 0;
      }
      const { text } = await rg.buildChecklist(gh, plan);
      const file = rg.writeChecklist(root, plan, text);
      console.log(`checklist: ${file}`);
      console.log(`after the human pass: tools\\cli promote --to ${plan.target.slice(0, 12)} --confirm`);
      return 0;
    }

    // --confirm: refuse a failed item whose PR is inside the range, then move stable, then the issues.
    const { issues, manualIssues } = await rg.buildChecklist(gh, plan);
    const inRange = new Set(issues);
    const blocked = fail.filter((n) => inRange.has(n));
    if (blocked.length) {
      console.log(`FAIL  ${blocked.map((n) => "#" + n).join(", ")} failed but ships in this range. Fix or revert on main (promote --revert <PR>), or pass an earlier --to. stable not moved.`);
      for (const n of blocked) await rg.markRegression(gh, n, log);
      return 1;
    }
    if (plan.commits.length === 0) {
      console.log("nothing to promote: stable already contains the target");
      return 0;
    }
    // The quorum rule (#575): when on, the issues of PRs that list manual steps need enough roster testers.
    const q = loadQuorum(root);
    if (q.enabled) {
      const conf = readConfirmations(val(args, "--confirmations"));
      if (conf.sha && !plan.target.startsWith(String(conf.sha)) && !String(conf.sha).startsWith(plan.target.slice(0, 8))) {
        throw new Error(`confirmations were collected on ${conf.sha}, not ${plan.target.slice(0, 8)}`);
      }
      const verdict = evaluateQuorum({ issues: manualIssues, results: conf.results, roster: q.roster, size: q.size });
      for (const f of verdict.failed) console.log(`FAIL  #${f.issue}: ${f.fail} tester(s) failed it. Run promote --fail ${f.issue}, fix, retest.`);
      for (const s of verdict.short) console.log(`FAIL  #${s.issue}: ${s.pass} of ${s.need} confirmations from roster testers.`);
      if (!verdict.ok) {
        console.log("quorum not met. stable not moved.");
        return 1;
      }
      log(`quorum met: ${verdict.met.length} issue(s) confirmed by ${q.size} or more roster testers`);
    }
    const tag = rg.moveStable(root, plan);
    console.log(`stable is now ${plan.target.slice(0, 8)} (tag ${tag})`);
    // An issue another, later PR on main also refs stays in-dev: that PR is not verified yet.
    const later = new Set();
    for (const n of plan.afterPrs) for (const i of rg.parseRefs((await gh.getPr(n))?.body)) later.add(i);
    for (const n of issues) {
      if (later.has(n)) log(`#${n}: also refs a PR merged after ${plan.target.slice(0, 8)}; left open`);
      else await rg.markVerified(gh, n, tag, log);
    }
    for (const n of fail) await rg.markRegression(gh, n, log);
    return 0;
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

async function revertCmd(root, gh, n) {
  if (!Number.isInteger(n) || n <= 0) throw new Error("--revert takes a PR number");
  const sha = rg.findMergeCommit(root, n);
  if (!sha) throw new Error(`PR #${n} is not on origin/main's first-parent history`);
  const stable = rg.resolveSha(root, "origin/stable");
  if (stable && rg.isAncestor(root, sha, stable)) {
    console.log(`note  PR #${n} is already on stable; the revert will reach stable on the next promotion`);
  }
  const slug = `revert-pr${n}`;
  const r = wt.wtNew(root, slug, {});
  rg.revertMerge(r.path, sha);
  const push = wt.git(r.path, ["push", "-u", "origin", r.branch]);
  if (push.status !== 0) throw new Error("push: " + (push.stderr || push.stdout));
  const old = await gh.getPr(n);
  const pr = wt.createPrGh(r.path, {
    head: r.branch,
    base: "main",
    title: `Revert #${n}${old ? ": " + old.title : ""}`,
    body: `Reverts #${n} on main before promotion (tools\\cli promote --revert ${n}).\n\n🤖 Generated with [Claude Code](https://claude.com/claude-code)`,
  });
  console.log(`PR: ${pr.url}`);
  wt.autoMergePrGh(r.path, pr.number);
  console.log(`revert queued. Prune after merge: tools\\cli wt prune ${slug}`);
  return 0;
}
