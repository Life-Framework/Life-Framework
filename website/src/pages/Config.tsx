import { useMemo, useState, type ReactNode } from "react";
import { GoLink, PageHeader } from "@/components/ui";
import { blob } from "@/lib/content";
import { useData } from "@/lib/data";
import { checkConfig, defaultOf, exportText, fieldErrors, importConfig, isRecordSchema, type Schema } from "@/lib/configgen";
import { parseJson } from "../../../tools/config/rules.mjs";

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

// parseJson keeps positions; the form wants plain values.
const plainOf = (n: any): unknown =>
  n.t === "obj" ? Object.fromEntries(n.props.map((p: any) => [p.key, plainOf(p.val)])) : n.t === "arr" ? n.items.map(plainOf) : n.value;

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
  const [extras, setExtras] = useState<Obj>({});
  const [keep, setKeep] = useState<Set<string>>(new Set());
  const [paste, setPaste] = useState("");
  const [importNote, setImportNote] = useState("");
  const json = useMemo(() => exportText(schema, value, extras, keep), [schema, value, extras, keep]);
  // The very rules `tools\cli config-check` applies, run over the file this page would export.
  const problems = useMemo(() => checkConfig(schema, json), [schema, json]);
  const errors = useMemo(() => fieldErrors(problems), [problems]);
  const bad = problems.filter((p) => p.level === "error").length;
  const hasExtras = Object.keys(extras).length > 0;

  // Import: the file lands in the form; what the form cannot hold is kept and written back on export.
  const load = (text: string) => {
    let root;
    try {
      root = parseJson(text);
    } catch {
      const found = checkConfig(schema, text, "the imported file")[0];
      setImportNote(`${found.msg} (line ${found.line}, column ${found.col}). Nothing was imported.`);
      return;
    }
    const raw = JSON.parse(JSON.stringify(plainOf(root)));
    const got = importConfig(schema, raw);
    setValue(got.value);
    setExtras(got.extras);
    setKeep(got.keep);
    setImportNote("Imported. Values outside the schema are marked below; keys this build does not know are kept in the export.");
  };
  const onFile = async (f?: File | null) => {
    if (f) load(await f.text());
  };

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
        <section aria-labelledby="sec-import" className="sheet p-4 space-y-3">
          <h2 id="sec-import" className="display-3">
            Start from your current file
          </h2>
          <p className="muted text-[0.9375rem]">Drop in your existing config.json or paste it. It stays in your browser.</p>
          <input type="file" accept=".json,application/json" aria-label="Choose a config.json" onChange={(e) => onFile(e.target.files?.[0])} />
          <textarea
            rows={4}
            value={paste}
            onChange={(e) => setPaste(e.target.value)}
            placeholder="or paste the file here"
            aria-label="Paste a config.json"
            className={`${inputCls} max-w-xl`}
          />
          <div>
            <button type="button" className="btn btn-ghost" disabled={!paste.trim()} onClick={() => load(paste)}>
              Import pasted file
            </button>
          </div>
          {importNote && <p role="status" className="text-[0.875rem] font-semibold">{importNote}</p>}
        </section>
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
            {bad} error{bad === 1 ? "" : "s"}. The server would refuse {bad === 1 ? "it" : "them"}, so the download stays off until {bad === 1 ? "it is" : "they are"} fixed.
          </p>
        )}
        {problems.length > 0 && (
          <ul className="text-[0.8125rem] space-y-1 max-h-[30vh] overflow-auto" aria-label="What config-check reports">
            {problems.map((p, n) => (
              <li key={n} className={p.level === "error" ? "text-stamp" : "muted"}>
                <span className="font-mono font-semibold">{p.level === "error" ? "ERROR" : "WARN"}</span> {p.msg}
              </li>
            ))}
          </ul>
        )}
        {hasExtras && <p className="text-[0.8125rem] muted">Keys this page cannot edit were kept from your file and are written back on download.</p>}
        <div className="flex flex-wrap gap-3">
          <button type="button" className="btn btn-primary" onClick={download} disabled={bad > 0}>
            Download config.json
          </button>
          <button type="button" className="btn btn-ghost" onClick={() => { setValue(defaultOf(schema) as Obj); setExtras({}); setKeep(new Set()); setImportNote(""); }}>
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
        <GoLink to="/config/reference">Field reference</GoLink>
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
