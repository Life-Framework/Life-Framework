import { AnchorLink, GoLink, PageHeader } from "@/components/ui";
import { blob } from "@/lib/content";
import { useData } from "@/lib/data";
import { isRecordSchema, type Schema } from "@/lib/configgen";

// /config/reference: every field of the owner config, listed from config.schema.json at build time
// (scripts/build-content.mjs -> src/generated/data/config-reference.json), so it cannot drift.

type Reference = { schema: Schema; example: Record<string, unknown> };

const range = (s: Schema) => {
  if (s.minimum === undefined && s.maximum === undefined) return "";
  return `${s.minimum ?? "…"} to ${s.maximum ?? "…"}`;
};
const typeOf = (s: Schema) => (s.type === "array" ? `list of ${s.items?.type ?? "value"}` : (s.type ?? ""));
const defaultText = (s: Schema) => (s.default === undefined ? "" : JSON.stringify(s.default));

function FieldTable({ schema, prefix }: { schema: Schema; prefix: string }) {
  const rows = Object.entries(schema.properties ?? {}).filter(([, p]) => !isRecordSchema(p));
  if (!rows.length) return null;
  return (
    <div className="overflow-x-auto mt-3">
      <table className="w-full text-left text-[0.875rem] border-collapse">
        <thead>
          <tr className="border-b-2 border-ink font-mono text-[0.75rem] uppercase tracking-wide">
            <th className="py-2 pr-4">Field</th>
            <th className="py-2 pr-4">Type</th>
            <th className="py-2 pr-4">Default</th>
            <th className="py-2 pr-4">Range</th>
            <th className="py-2">What it does</th>
          </tr>
        </thead>
        <tbody>
          {rows.map(([k, p]) => (
            <tr key={k} id={`${prefix}.${k}`} className="border-b border-rule align-top">
              <td className="py-2 pr-4 font-mono font-semibold break-all">{k}</td>
              <td className="py-2 pr-4 font-mono">{typeOf(p)}</td>
              <td className="py-2 pr-4 font-mono">{defaultText(p)}</td>
              <td className="py-2 pr-4 font-mono whitespace-nowrap">{range(p)}</td>
              <td className="py-2 muted">{p.description ?? ""}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function Records({ name, schema, prefix, keyFormat }: { name: string; schema: Schema; prefix: string; keyFormat?: string[] }) {
  const entry = schema.additionalProperties as Schema;
  return (
    <div className="mt-6" id={`${prefix}.${name}`}>
      <h4 className="font-mono text-[0.9375rem] font-semibold">
        {name} <span className="muted font-sans font-normal">(records, keyed by a stable key)</span>
      </h4>
      {schema.description && <p className="muted text-[0.9375rem] mt-1 max-w-3xl">{schema.description}</p>}
      {keyFormat && (
        <>
          <p className="kicker mt-3">Record key formats</p>
          <ul className="mt-1 space-y-1 text-[0.9375rem]">
            {keyFormat.map((f) => (
              <li key={f} className="font-mono">
                {f}
              </li>
            ))}
          </ul>
        </>
      )}
      <p className="kicker mt-3">Fields of each record</p>
      <FieldTable schema={entry} prefix={`${prefix}.${name}.*`} />
    </div>
  );
}

// What the schema cannot say: the grammar of each record set's key. Mirrors tools/config/check.mjs.
const KEY_FORMATS: Record<string, string[]> = {
  "shops.priceRules": [
    "Prefabs/Items/Food/  (a prefab-path substring; every prefab whose path contains it matches)",
    "prefab:<GUID>  (one prefab, by its 16 hex digit GUID)",
    "type:<flags>,mode:<mode>  (an arsenal type; mode defaults to 2, any)",
  ],
  "vehicles.vehicles": ["54C3CC22DEBD57BE  (the vehicle's prefab GUID, 16 hex digits)"],
};

function Section({ name, schema, example }: { name: string; schema: Schema; example: unknown }) {
  const records = Object.entries(schema.properties ?? {}).filter(([, p]) => isRecordSchema(p));
  return (
    <section id={name} aria-labelledby={`h-${name}`} className="scroll-mt-24">
      <h2 id={`h-${name}`} className="display-3 rule-top pt-3">
        {name}
        <AnchorLink id={name} label={name} />
      </h2>
      {schema.description && <p className="muted mt-1">{schema.description}</p>}
      <FieldTable schema={schema} prefix={name} />
      {records.map(([k, p]) => (
        <Records key={k} name={k} schema={p} prefix={name} keyFormat={KEY_FORMATS[`${name}.${k}`]} />
      ))}
      {example !== undefined && (
        <>
          <p className="kicker mt-6">Example</p>
          <pre className="terminal p-4 font-mono text-[0.8125rem] overflow-auto mt-1">{JSON.stringify({ [name]: example }, null, 2)}</pre>
        </>
      )}
    </section>
  );
}

const ConfigReference = () => {
  const ref = useData<Reference>("config-reference");
  const sections = ref ? Object.entries(ref.schema.properties ?? {}).filter(([, p]) => p.type === "object") : [];
  return (
    <>
      <PageHeader kicker="For server owners" title="Every field of your config">
        <p>
          The owner file is <code>$profile:LifeFramework/config.json</code>. Every section below lists each field with its type, default,
          range and meaning. A value you leave out keeps the shipped default, so write only what you change.
        </p>
      </PageHeader>
      <div className="wrap max-w-5xl pb-16">
        {!ref ? (
          <p className="muted">{ref === null ? "The schema is missing from this build." : "Loading the schema."}</p>
        ) : (
          <div className="space-y-12">
            <nav aria-label="Config sections" className="flex flex-wrap gap-x-6 gap-y-2">
              {sections.map(([k]) => (
                <a key={k} className="link font-mono text-[0.9375rem]" href={`#${k}`}>
                  {k}
                </a>
              ))}
              <a className="link text-[0.9375rem]" href="#check">
                Check a file
              </a>
            </nav>
            <p className="muted">
              Build the file in the browser with the <GoLink to="/config">config generator</GoLink>, or write it by hand. A file starts with{" "}
              <code>schemaVersion</code> (currently {String(ref.schema.properties?.schemaVersion?.default ?? 1)}).
            </p>
            {sections.map(([k, s]) => (
              <Section key={k} name={k} schema={s} example={ref.example[k]} />
            ))}
            <section id="check" aria-labelledby="h-check" className="scroll-mt-24">
              <h2 id="h-check" className="display-3 rule-top pt-3">
                Check a file before a restart
                <AnchorLink id="check" label="Check a file" />
              </h2>
              <p className="muted mt-1 max-w-3xl">
                The server refuses a bad value, skips a bad section and ignores an unknown key, and tells you only in its log.{" "}
                <code>config-check</code> reports the same problems first, offline, with line and column, using this schema.
              </p>
              <pre className="terminal p-4 font-mono text-[0.8125rem] overflow-auto mt-3">{`tools\\cli config-check path\\to\\config.json

ERROR  5:24  shops.defaultBuyPrice is not a whole number; keeping the shipped value
WARN   7:5  shops.bogus is not a known key; ignored`}</pre>
              <p className="muted mt-3 max-w-3xl">
                Exit code 1 on any error. <code>--strict</code> fails on warnings too, <code>--json</code> prints the problems for a script.
                Full usage is in{" "}
                <a className="link" href={blob("docs/config/owner-config.md")}>
                  owner-config.md
                </a>
                .
              </p>
            </section>
            <p className="text-sm muted">
              Generated from{" "}
              <a className="link" href={blob("docs/config/config.schema.json")}>
                config.schema.json
              </a>{" "}
              and{" "}
              <a className="link" href={blob("docs/config/config.example.json")}>
                config.example.json
              </a>{" "}
              at build time.
            </p>
            <GoLink to="/servers">Back to server owners</GoLink>
          </div>
        )}
      </div>
    </>
  );
};

export default ConfigReference;
