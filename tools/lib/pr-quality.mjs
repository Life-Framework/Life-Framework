// What `wt ship` checks in a PR body before it runs the gate (docs/architecture/agent-quality.md).
//
// Three things used to slip through because nothing looked:
//   1. A change a player can see shipped with no picture, or with a path to a picture
//      instead of the picture. The maintainer then found the jank in a trailer.
//   2. A reviewer's findings stayed in a subagent's reply and never reached the PR.
//   3. A scaffold or template placeholder was left in the body.
//
// Pure functions: the selftest is tools/validation/pr-quality-selftest.mjs.

export const MARK = "TODO(scaffold)";

// Paths whose change a player can see. Scripts alone do not count: a rule change has
// no picture. UI code does, because the sheet is the feature.
const VISIBLE = [
  /^addons\/[^/]+\/Worlds\/.+\.layer$/,
  /^addons\/[^/]+\/Prefabs\/.+\.et$/,
  /^addons\/[^/]+\/Assets\/.+\.(xob|emat|edds|fbx)$/,
  /^addons\/[^/]+\/UI\/.+\.(layout|imageset|edds)$/,
  /^addons\/[^/]+\/Scripts\/Game\/Feature\/UIKit\//,
  /^addons\/[^/]+\/Scripts\/Game\/Feature\/.+(Sheet|Card|Screen|Menu|Hud|HUD|Form|Face)\.c$/,
  /^addons\/[^/]+\/Configs\/Signs\//,
];

export function visibleFiles(files) {
  return (files ?? []).filter((f) => VISIBLE.some((re) => re.test(f.replace(/\\/g, "/"))));
}

function strip(body) {
  return String(body ?? "").replace(/\r\n/g, "\n").replace(/<!--[\s\S]*?-->/g, "");
}

// Markdown or HTML images embedded in the body: [{ alt, url }].
export function embeddedImages(body) {
  const text = strip(body);
  const out = [];
  for (const m of text.matchAll(/!\[([^\]]*)\]\(([^)\s]+)[^)]*\)/g)) out.push({ alt: m[1], url: m[2] });
  for (const m of text.matchAll(/<img\b[^>]*\bsrc=["']([^"']+)["'][^>]*>/gi)) out.push({ alt: "", url: m[1] });
  return out;
}

// The repo path an image URL points at, or null for an image hosted elsewhere.
export function repoPathOf(url) {
  const raw = /^https:\/\/raw\.githubusercontent\.com\/[^/]+\/[^/]+\/[^/]+\/(.+)$/.exec(url);
  if (raw) return decodeURIComponent(raw[1].split(/[?#]/)[0]);
  const blob = /^https:\/\/github\.com\/[^/]+\/[^/]+\/(?:blob|raw)\/[^/]+\/(.+)$/.exec(url);
  if (blob) return decodeURIComponent(blob[1].split(/[?#]/)[0]);
  if (!/^[a-z]+:/i.test(url)) return url.replace(/^\.?\//, "");
  return null;
}

// "No screenshots: <reason>" in the body waives the picture, in the open, where the
// maintainer reads it. The reason must be a real sentence.
export function shotWaiver(body) {
  const m = /^\s*(?:\*\*)?no screenshots?(?:\*\*)?\s*:\s*(.+)$/im.exec(strip(body));
  return m && m[1].replace(/[*_`]/g, "").trim().length >= 15 ? m[1].trim() : null;
}

function section(body, re) {
  const lines = strip(body).split("\n");
  const start = lines.findIndex((l) => /^#{1,6}\s/.test(l) && re.test(l));
  if (start === -1) return null;
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i++) {
    if (/^#{1,6}\s/.test(lines[i])) {
      end = i;
      break;
    }
  }
  return lines.slice(start + 1, end).join("\n").trim();
}

// Open findings in a review file: "- [ ] ..." lines. A resolved finding is "- [x] ... fixed: ..."
// or "- [x] ... dismissed: <why>".
export function reviewState(reviewText) {
  if (reviewText === null || reviewText === undefined) return { exists: false, open: [], closed: 0, unexplained: [] };
  const open = [];
  const unexplained = [];
  let closed = 0;
  for (const line of String(reviewText).replace(/\r\n/g, "\n").split("\n")) {
    const m = /^\s*[-*]\s*\[( |x|X)\]\s*(.+)$/.exec(line);
    if (!m) continue;
    if (m[1] === " ") open.push(m[2].trim());
    else {
      closed++;
      if (!/\b(fixed|dismissed)\b\s*:?\s*\S/i.test(m[2])) unexplained.push(m[2].trim());
    }
  }
  return { exists: true, open, closed, unexplained };
}

// The body `wt ship` sends: the review file is appended under "## Review" when the body
// has no such section, so the findings travel with the PR.
export function withReview(body, reviewText) {
  if (reviewText === null || reviewText === undefined || !String(reviewText).trim()) return body;
  if (section(body, /\breview\b/i) !== null && reviewState(section(body, /\breview\b/i)).closed > 0) return body;
  const block = `## Review findings\n\n${String(reviewText).replace(/\r\n/g, "\n").trim()}\n`;
  const footer = /\n🤖 Generated with[^\n]*\n?$/.exec(body);
  if (footer) return `${body.slice(0, footer.index).trimEnd()}\n\n${block}${footer[0]}`;
  return `${body.trimEnd()}\n\n${block}`;
}

// Problems that stop the ship. `files` is the PR's changed paths, `exists(path)` says whether
// a repo path is in the tree being shipped, `reviewText` is tmp/review-<slug>.md or null.
export function bodyProblems({ body, files, exists = () => true, reviewText = null, hasBodyFile = true }) {
  const problems = [];
  const game = (files ?? []).some((f) => f.replace(/\\/g, "/").startsWith("addons/"));
  const visible = visibleFiles(files);
  const text = strip(body);

  if (game && !hasBodyFile) {
    problems.push("this PR changes addons/ but has no --body-file. Write tmp/pr-body-<slug>.md (tools\\cli feature new makes one; the sections are in .github/PULL_REQUEST_TEMPLATE.md)");
    return problems;
  }
  if (text.includes(MARK)) {
    const n = text.split(MARK).length - 1;
    problems.push(`the PR body still has ${n} ${MARK} placeholder(s); replace each with the real text`);
  }
  if (game && section(body, /\bmanual\b/i) === null) {
    problems.push('the PR body has no "## Manual testing" section. List the steps a human takes, or write "None" with the reason');
  }
  if (visible.length) {
    const images = embeddedImages(body);
    const waiver = shotWaiver(body);
    if (!images.length && !waiver) {
      problems.push(
        `a player can see this change (${visible.slice(0, 3).join(", ")}${visible.length > 3 ? `, +${visible.length - 3} more` : ""}) but the PR body embeds no picture. ` +
          "Take `tools\\cli look <area>` (or `look item <prefab>`, or `wb_preview`) before and after, keep them with `tools\\cli evidence <issue> before|after <area|png>`, and paste the table it prints. No game session is needed for a world, prop, sign, item or prefab. " +
          'If there is truly nothing to see, write a line "No screenshots: <reason>"',
      );
    }
    for (const img of images) {
      const p = repoPathOf(img.url);
      if (p && /^tmp\//.test(p.replace(/\\/g, "/"))) {
        // `look` writes tmp/look/<area>/*.png and `wb_preview` a temp PNG; tmp/ is gitignored, so the image would be
        // broken in the PR even though the file exists here. `evidence` copies it to docs-images/, which is committed.
        problems.push(`the PR body embeds ${p}, which is under tmp/ (gitignored, never pushed). Keep the picture with \`tools\\cli evidence <issue> before|after <area|png>\` and paste the table it prints`);
      } else if (p && !exists(p)) problems.push(`the PR body embeds ${p}, which is not in this branch. Commit the picture (tools\\cli evidence writes it to website/public/docs-images/)`);
    }
  }
  const review = reviewState(reviewText);
  if (review.open.length) {
    problems.push(`the review file has ${review.open.length} open finding(s); tick each as "fixed: <what>" or "dismissed: <why>": ${review.open.slice(0, 3).map((f) => `"${f.slice(0, 70)}"`).join("; ")}`);
  }
  if (review.unexplained.length) {
    problems.push(`${review.unexplained.length} ticked review finding(s) do not say "fixed:" or "dismissed:" with a reason: "${review.unexplained[0].slice(0, 70)}"`);
  }
  return problems;
}
