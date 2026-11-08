import { useState, type ReactNode } from "react";
import { Link } from "react-router-dom";
import { Arrow, External } from "@/components/icons";
import { formatDate, githubData, issueStatus, milestoneName, splitTitle, stillSrc, stillSrcSet, type Clip, type Issue, type Still, siteData, still as stillById } from "@/lib/content";

export const Stamp = ({ children, tone = "red", tilt = false }: { children: ReactNode; tone?: "red" | "green" | "ink"; tilt?: boolean }) => (
  <span className={`stamp ${tone === "green" ? "stamp-green" : tone === "ink" ? "stamp-ink" : ""} ${tilt ? "stamp-tilt" : ""}`}>{children}</span>
);

export function PageHeader({ kicker, title, children }: { kicker: string; title: ReactNode; children?: ReactNode }) {
  return (
    <div className="wrap pt-12 md:pt-16 pb-10">
      <p className="kicker mb-4">{kicker}</p>
      <h1 className="display-1 max-w-4xl">{title}</h1>
      {children && <div className="lede mt-6 space-y-3">{children}</div>}
    </div>
  );
}

export function Section({ id, kicker, title, children, aside }: { id?: string; kicker: string; title: ReactNode; children: ReactNode; aside?: ReactNode }) {
  return (
    <section id={id} aria-labelledby={id ? `${id}-title` : undefined} className="wrap py-14 md:py-20">
      <div className="rule-top pt-4 mb-10 flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="kicker mb-2">{kicker}</p>
          <h2 id={id ? `${id}-title` : undefined} className="display-2">
            {title}
          </h2>
        </div>
        {aside}
      </div>
      {children}
    </section>
  );
}

// An internal or external link with the right element and an arrow.
export function GoLink({ to, children, className = "" }: { to: string; children: ReactNode; className?: string }) {
  const inner = (
    <>
      {children}
      {to.startsWith("/") ? <Arrow /> : <External className="w-4 h-4" />}
    </>
  );
  const cls = `inline-flex items-center gap-1.5 font-mono text-[0.8125rem] uppercase tracking-[0.06em] link link-green ${className}`;
  return to.startsWith("/") ? (
    <Link to={to} className={cls}>
      {inner}
    </Link>
  ) : (
    <a href={to} className={cls}>
      {inner}
    </a>
  );
}

function Credit({ date, source, sourceUrl, wip, where }: { date: string; source: string; sourceUrl: string; wip: boolean; where?: string }) {
  return (
    <span className="flex flex-wrap items-center gap-x-3 gap-y-1 mt-1.5 font-mono text-[0.7rem] uppercase tracking-[0.06em] text-ink-2">
      {wip && <span className="text-stamp font-medium">Work in progress</span>}
      {where && <span>{where}</span>}
      <span>{formatDate(date)}</span>
      <a href={sourceUrl} className="link">
        {source}
      </a>
    </span>
  );
}

export function Figure({ still, eager = false, sizes = "(min-width: 1024px) 50vw, 100vw", className = "" }: { still: Still; eager?: boolean; sizes?: string; className?: string }) {
  return (
    <figure className={`photo ${className}`}>
      <img
        src={stillSrc(still)}
        srcSet={stillSrcSet(still)}
        sizes={sizes}
        width={still.width}
        height={still.height}
        alt={still.alt}
        loading={eager ? "eager" : "lazy"}
        decoding="async"
        {...(eager ? { fetchpriority: "high" } : {})}
      />
      <figcaption>
        {still.caption}
        <Credit {...still} />
      </figcaption>
    </figure>
  );
}

// A clip from the film-evidence release. Nothing loads until the visitor presses play.
export function ClipPlayer({ clip }: { clip: Clip }) {
  const poster = stillById(clip.poster);
  const [playing, setPlaying] = useState(false);
  return (
    <figure className="photo">
      {playing && clip.src ? (
        <video src={clip.src} poster={stillSrc(poster)} controls autoPlay muted playsInline width={1280} height={720} className="aspect-video" />
      ) : (
        <div className="relative">
          <img src={stillSrc(poster)} srcSet={stillSrcSet(poster)} sizes="(min-width: 1024px) 60vw, 100vw" width={1280} height={720} alt="" loading="lazy" decoding="async" className="aspect-video object-cover" />
          {clip.src ? (
            <button type="button" onClick={() => setPlaying(true)} className="absolute inset-0 flex items-center justify-center group" aria-label={`Play the clip: ${clip.title}`}>
              <span className="btn btn-primary pointer-events-none">Play the clip · 0:20</span>
            </button>
          ) : (
            <a href={clip.release} className="absolute inset-0 flex items-center justify-center">
              <span className="btn btn-primary">Watch the clip on GitHub</span>
            </a>
          )}
        </div>
      )}
      <figcaption>
        <strong className="font-semibold">{clip.title}.</strong> {clip.caption}
        <Credit {...clip} />
      </figcaption>
    </figure>
  );
}

// One cell per scope in the milestone: filled when the issue is closed.
export function ScopeMeter({ issues, label }: { issues: Issue[]; label: string }) {
  const sorted = [...issues].sort((a, b) => Number(b.state === "closed") - Number(a.state === "closed"));
  const done = issues.filter((i) => i.state === "closed").length;
  return (
    <div
      role="img"
      aria-label={`${label}: ${done} of ${issues.length} scopes done`}
      className="grid gap-[3px]"
      style={{ gridTemplateColumns: `repeat(${Math.max(issues.length, 1)}, minmax(0, 1fr))` }}
    >
      {sorted.map((i) => (
        <span key={i.number} className={`block h-7 border border-ink ${i.state === "closed" ? "bg-green" : "bg-transparent"}`} />
      ))}
    </div>
  );
}

// The milestones as stops on a route; the current one is "you are here".
export function RouteMap({ current, compact = false }: { current: string; compact?: boolean }) {
  const ms = githubData.milestones;
  const idx = ms.findIndex((m) => m.id === current);
  return (
    <ol className="relative grid gap-0 md:grid-cols-5">
      {ms.map((m, i) => {
        const state = m.state === "closed" || (idx >= 0 && i < idx) ? "done" : i === idx ? "here" : "next";
        return (
          <li key={m.id} className="relative pl-10 md:pl-0 md:pt-10 pb-6 md:pb-0 md:pr-4">
            {i < ms.length - 1 && (
              <span
                aria-hidden="true"
                className={`absolute left-[13px] top-[15px] bottom-0 w-[4px] md:left-[15px] md:right-0 md:top-[13px] md:bottom-auto md:h-[4px] md:w-auto ${i < idx ? "bg-ink" : "bg-rule"}`}
              />
            )}
            <span
              aria-hidden="true"
              className={`absolute left-0 top-0 md:left-0 md:top-0 w-[30px] h-[30px] rounded-full border-[4px] ${
                state === "here" ? "bg-sign border-ink" : state === "done" ? "bg-ink border-ink" : "bg-paper border-rule"
              }`}
            />
            <span className="kicker block">
              {m.id}
              {state === "here" && <span className="text-stamp"> · you are here</span>}
              {state === "done" && <span> · done</span>}
            </span>
            <span className="display-3 block mt-1">{milestoneName(m)}</span>
            {!compact && <span className="block mt-2 text-[0.9375rem] muted leading-snug">{m.description}</span>}
          </li>
        );
      })}
    </ol>
  );
}

export function IssueLedger({ issues, showExcerpt = true, showStatus = true }: { issues: Issue[]; showExcerpt?: boolean; showStatus?: boolean }) {
  return (
    <ul className="border-t-2 border-ink">
      {issues.map((i) => {
        const { code, title } = splitTitle(i.title);
        const s = issueStatus(i);
        return (
          <li key={i.number} className={`border-b border-rule py-4 grid gap-x-6 gap-y-1 ${showStatus ? "sm:grid-cols-[6.5rem_1fr_auto]" : "sm:grid-cols-[6.5rem_1fr]"}`}>
            <span className="font-mono text-[0.8125rem] text-ink-2">
              {code ?? `#${i.number}`}
              {code && <span className="block text-[0.75rem]">#{i.number}</span>}
            </span>
            <span className="min-w-0">
              <a href={i.url} className="heading link hover:text-green">
                {title}
              </a>
              {showExcerpt && i.excerpt && <span className="block mt-1 text-[0.9375rem] muted leading-snug">{i.excerpt}</span>}
              {i.assignees.length > 0 && i.state === "open" && (
                <span className="block mt-1 font-mono text-[0.75rem] text-ink-2">on it: {i.assignees.join(", ")}</span>
              )}
            </span>
            {showStatus && (
              <span className="sm:text-right pt-1">
                <Stamp tone={s.tone}>{s.label}</Stamp>
              </span>
            )}
          </li>
        );
      })}
    </ul>
  );
}

export function SourceNote({ source, fetchedAt }: { source: string; fetchedAt: string }) {
  return (
    <p className="font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2">
      Read from GitHub {formatDate(fetchedAt)}
      {source === "snapshot" ? " · GitHub was unreachable at build time, so this is the committed snapshot" : " at build time"}
    </p>
  );
}
