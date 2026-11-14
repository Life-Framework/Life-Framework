import { GoLink } from "@/components/ui";
import { REPO_URL, blob, tree } from "@/lib/content";

// "Run your own": the three things a server owner needs, as one block. It is a component so the Join page,
// and later the Workshop and Discord copy, say the same thing from one place. Link targets: the example
// server folder (server/example), the config generator (/config) and the install guide (docs/owner/install.md).

const STEPS: { title: string; body: string; to: string; label: string }[] = [
  {
    title: "Start from the example server",
    body: "A ready folder: the core, the 1989 Everon pack, the server config and one start command for Windows and Linux.",
    to: tree("server/example"),
    label: "Example server",
  },
  {
    title: "Set your numbers",
    body: "Change prices, wages and autosave in the browser. It checks the file as you go and downloads only what you changed.",
    to: "/config",
    label: "Config generator",
  },
  {
    title: "Install it from zero",
    body: "Install the dedicated server, open the ports, start it, and keep the save across restarts.",
    to: blob("docs/owner/install.md"),
    label: "Install guide",
  },
];

export function RunYourOwn() {
  return (
    <div className="max-w-6xl" data-block="run-your-own">
      <p className="max-w-prose mb-8">
        Life Framework is open source. You do not need our server: take the framework and run your own, with your own rules, name
        and economy.
      </p>
      <ol className="grid gap-8 md:grid-cols-3 list-none p-0">
        {STEPS.map((s, i) => (
          <li key={s.to}>
            <p className="kicker">Step {i + 1}</p>
            <h3 className="heading mb-2">{s.title}</h3>
            <p className="mb-3">{s.body}</p>
            <GoLink to={s.to}>{s.label}</GoLink>
          </li>
        ))}
      </ol>
      <p className="mt-8">
        <GoLink to={REPO_URL}>The framework on GitHub</GoLink>
      </p>
    </div>
  );
}
