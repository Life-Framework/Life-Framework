import { GoLink, PageHeader } from "@/components/ui";
import { blob } from "@/lib/content";
import rules from "../../content/rules.json";

// The text is website/content/rules.json, written by `tools\cli rules export` from the same pack strings the town
// board shows, so the site, the board and the Discord copy cannot drift. The orientation quiz is a separate subset.
const Rules = () => (
  <>
    <PageHeader kicker="Server rules" title={rules.title}>
      {rules.intro && <p>{rules.intro}</p>}
    </PageHeader>
    <div className="wrap max-w-4xl">
      <ol className="space-y-4" aria-label="The rules">
        {rules.rules.map((rule, i) => (
          <li key={rule} className="sheet flex gap-4 p-5">
            <span className="plate shrink-0">{i + 1}</span>
            <span>{rule}</span>
          </li>
        ))}
      </ol>
      {rules.next && <p className="mt-10 lede">{rules.next}</p>}
      <p className="mt-12 pt-6 border-t border-rule flex flex-wrap gap-x-6 gap-y-2">
        <GoLink to={blob("website/content/rules.json")}>Source file</GoLink>
        <GoLink to={blob("docs/owner/install.md")}>Run your own server: install guide</GoLink>
        <GoLink to="/servers">For server owners</GoLink>
      </p>
    </div>
  </>
);

export default Rules;
