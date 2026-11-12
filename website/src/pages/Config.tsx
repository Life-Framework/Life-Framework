import { useMemo, useState, type ReactNode } from "react";
import { GoLink, PageHeader } from "@/components/ui";
import { blob } from "@/lib/content";
import { useData } from "@/lib/data";
import { defaultOf, isRecordSchema, minimalDiff, validate, type Schema } from "@/lib/configgen";

type Obj = Record<string, unknown>;

const inputCls = "border-2 border-ink bg-paper px-2 py-1.5 font-mono text-[0.875rem] w-full max-w-xs";
const hint = (s: Schema) => {
  const bits: string[] = [];
  if (s.minimum !== undefined || s.maximum !== undefined) bits.push(`range ${s.minimum ?? "…"} to ${s.maximum ?? "…"}`);
  if (s.default !== undefined && s.type !== "array") bits.push(`default ${JSON.stringify(s.default)}`);
  return bits.join(" · ");
};

function Field({ name, id, schema, value, error, onChange }: { name: string; id: string; schema: Schema; value: unknown; error?: string; onChange: (v: unknown) => void }) {
  let control;
  if (schema.type === "boolean") {
    control = <input id={id} type="checkbox" checked={!!value} onChange={(e) => onChange(e.target.checked)} className="w-5 h-5" />;
  } else if (schema.type === "integer" || schema.type === "number") {
    control = (
      <input
        id={id}
        type="number"
        step={schema.type === "integer" ? 1 : "any"}
        min={schema.minimum}
        max={schema.maximum}
        value={typeof value === "number" && !Number.isNaN(value) ? value : ""}
        onChange={(e) => onChange(e.target.value === "" ? NaN : Number(e.target.value))}
        className={inputCls}
      />
    );
  } else if (schema.type === "array") {
    control = (
      <textarea
        id={id}
        rows={3}
        value={((value as string[]) ?? []).join("\n")}
        onChange={(e) => onChange(e.target.value === "" ? [] : e.target.value.split("\n"))}
        className={`${inputCls} max-w-xl`}
      />
    );
  } else {
    control = <input id={id} type="text" value={(value as string) ?? ""} onChange={(e) => onChange(e.target.value)} className={`${inputCls} max-w-xl`} />;
  }
  return (
    <div className="py-3 border-b border-rule grid gap-x-6 gap-y-1 md:grid-cols-[12rem_1fr]">
      <label htmlFor={id} className="font-mono text-[0.8125rem] font-semibold break-all">
        {name}
      </label>
      <div>
        {control}
        {schema.description && <p className="muted text-[0.875rem] mt-1">{schema.description}</p>}
        <p className="font-mono text-[0.75rem] text-ink-2 mt-0.5">{schema.type === "array" ? "one per line" : hint(schema)}</p>
        {error && (
          <p role="alert" className="text-[0.875rem] text-stamp font-semibold mt-1">
            {error}
          </p>
        )}
      </div>
    </div>
  );
}

function Fields({ schema, value, path, errors, onChange }: { schema: Schema; value: Obj; path: string; errors: Record<string, string>; onChange: (v: Obj) => void }) {
  return (
    <>
      {Object.entries(schema.properties ?? {}).map(([k, p]) =>
        isRecordSchema(p) ? (
          <Records key={k} name={k} schema={p} value={(value[k] as Obj) ?? {}} path={`${path}.${k}`} errors={errors} onChange={(v) => onChange({ ...value, [k]: v })} />
        ) : (
          <Field key={`${path}.${k}`} id={`f${path}.${k}`} name={k} schema={p} value={value[k]} error={errors[`${path}.${k}`]} onChange={(v) => onChange({ ...value, [k]: v })} />
        ),
      )}
    </>
  );
}

// A keyed record set, such as shop price rules: add by key, edit, remove.
function Records({ name, schema, value, path, errors, onChange }: { name: string; schema: Schema; value: Obj; path: string; errors: Record<string, string>; onChange: (v: Obj) => void }) {
  const [draft, setDraft] = useState("");
  const entry = schema.additionalProperties as Schema;
  const key = draft.trim();
  const taken = key in value;
  const add = () => {
    if (!key || taken) return;
    onChange({ ...value, [key]: defaultOf(entry) });
    setDraft("");
  };
  return (
    <div className="py-4 border-b border-rule">
      <h4 className="font-mono text-[0.8125rem] font-semibold">{name}</h4>
      {schema.description && <p className="muted text-[0.875rem] mt-1 max-w-3xl">{schema.description}</p>}
      <ul className="mt-3 space-y-3">
        {Object.entries(value).map(([k, ev]) => (
          <li key={k} className="sheet p-4">
            <div className="flex items-baseline justify-between gap-4">
              <span className="font-mono text-[0.875rem] font-semibold break-all">{k}</span>
              <button type="button" className="link text-[0.875rem]" onClick={() => onChange(Object.fromEntries(Object.entries(value).filter(([x]) => x !== k)))}>
                Remove
              </button>
            </div>
            <Fields schema={entry} value={ev as Obj} path={`${path}.${k}`} errors={errors} onChange={(v) => onChange({ ...value, [k]: v })} />
          </li>
        ))}
      </ul>
      <div className="mt-3 flex flex-wrap items-center gap-3">
        <label className="sr-only" htmlFor={`add-${path}`}>
          New key for {name}
        </label>
        <input
          id={`add-${path}`}
          type="text"
          value={draft}
          placeholder="new key"
          onChange={(e) => setDraft(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") {
              e.preventDefault();
              add();
            }
          }}
          className={inputCls}
        />
        <button type="button" className="btn btn-ghost" onClick={add} disabled={!key || taken}>
          Add
        </button>
        {taken && <span className="text-[0.875rem] text-stamp font-semibold">That key already exists.</span>}
      </div>
    </div>
  );
}

const ConfigShell = ({ children }: { children: ReactNode }) => (
  <>
    <PageHeader kicker="For server owners" title="Build your server config">
      <p>
        Change what you want and download <code>config.json</code>. The file holds only the values you changed from the defaults,
        plus the schema version, so it stays short and keeps working when the defaults improve. Put it at{" "}
        <code>$profile:LifeFramework/config.json</code>. Nothing leaves your browser.
      </p>
    </PageHeader>
    <div className="wrap max-w-6xl pb-16">{children}</div>
  </>
);

function Form({ schema }: { schema: Schema }) {
  const [value, setValue] = useState<Obj>(() => defaultOf(schema) as Obj);
  const errors = useMemo(() => validate(schema, value), [schema, value]);
  const diff = useMemo(() => minimalDiff(schema, value), [schema, value]);
  const json = JSON.stringify(diff, null, 2) + "\n";
  const bad = Object.keys(errors).length;

  const download = () => {
    const url = URL.createObjectURL(new Blob([json], { type: "application/json" }));
    const a = document.createElement("a");
    a.href = url;
    a.download = "config.json";
    a.click();
    URL.revokeObjectURL(url);
  };

  const sections = Object.entries(schema.properties ?? {}).filter(([, p]) => p.type === "object");
  return (
    <div className="grid gap-10 lg:grid-cols-[1fr_22rem] lg:items-start">
      <div className="space-y-10 min-w-0">
        {sections.map(([k, s]) => (
          <section key={k} aria-labelledby={`sec-${k}`}>
            <h2 id={`sec-${k}`} className="display-3 rule-top pt-3">
              {k}
            </h2>
            {s.description && <p className="muted mt-1">{s.description}</p>}
            <Fields schema={s} value={value[k] as Obj} path={`.${k}`} errors={errors} onChange={(v) => setValue({ ...value, [k]: v })} />
          </section>
        ))}
      </div>
      <aside className="lg:sticky lg:top-24 space-y-3 min-w-0" aria-label="Your config.json">
        <p className="kicker">Your config.json</p>
        <pre className="terminal p-4 font-mono text-[0.8125rem] overflow-auto max-h-[60vh]">{json}</pre>
        {bad > 0 && (
          <p role="alert" className="text-stamp font-semibold text-[0.9375rem]">
            {bad} value{bad === 1 ? "" : "s"} outside the schema. Fix {bad === 1 ? "it" : "them"} before you download.
          </p>
        )}
        <div className="flex flex-wrap gap-3">
          <button type="button" className="btn btn-primary" onClick={download} disabled={bad > 0}>
            Download config.json
          </button>
          <button type="button" className="btn btn-ghost" onClick={() => setValue(defaultOf(schema) as Obj)}>
            Reset
          </button>
        </div>
        <p className="text-sm muted">
          Rendered from{" "}
          <a className="link" href={blob("docs/config/config.schema.json")}>
            config.schema.json
          </a>
          , read at build time.
        </p>
        <GoLink to="/servers">Back to server owners</GoLink>
      </aside>
    </div>
  );
}

const Config = () => {
  const schema = useData<Schema>("config-schema");
  return (
    <ConfigShell>
      {schema ? <Form schema={schema} /> : <p className="muted">{schema === null ? "The schema is missing from this build." : "Loading the schema."}</p>}
    </ConfigShell>
  );
};

export default Config;
