import { useState } from "react";
import { Link } from "react-router-dom";
import { GoLink, PageHeader, Section, Stamp } from "@/components/ui";
import { blob, formatDate, support, supportOpen, type Cost, type Wallet } from "@/lib/content";

function amountText(c: Cost) {
  if (c.status === "planned") return "Not yet";
  if (c.amount === null) return "Not filled in yet";
  if (c.amount === 0) return "Free";
  const n = c.amount.toLocaleString("en-US", { style: "currency", currency: "USD", maximumFractionDigits: 2 });
  return `${n} a ${c.period}`;
}

function CopyButton({ text }: { text: string }) {
  const [state, setState] = useState<"idle" | "copied" | "failed">("idle");
  return (
    <button
      type="button"
      className="btn btn-ghost"
      onClick={() => {
        if (!navigator.clipboard) {
          setState("failed");
          return;
        }
        navigator.clipboard.writeText(text).then(
          () => setState("copied"),
          () => setState("failed"),
        );
      }}
    >
      {state === "copied" ? "Copied" : state === "failed" ? "Select and copy it" : "Copy address"}
    </button>
  );
}

// QR codes need dark modules on a light field in both themes, so this one keeps its own colours.
const Qr = ({ w }: { w: Wallet }) => (
  <svg
    viewBox={`0 0 ${w.qr.size} ${w.qr.size}`}
    width={200}
    height={200}
    role="img"
    aria-label={`QR code of the ${w.asset} ${w.standard} address`}
    shapeRendering="crispEdges"
    className="border border-ink shrink-0"
  >
    <rect width={w.qr.size} height={w.qr.size} fill="#ffffff" />
    <path d={w.qr.path} fill="#000000" />
  </svg>
);

function WalletSheet({ w }: { w: Wallet }) {
  return (
    <article className="sheet p-6 md:p-8">
      <span className="plate text-xl">
        {w.asset} · {w.standard}
      </span>
      <h3 className="display-3 mt-4">
        {w.asset} on {w.network}
      </h3>
      <div className="mt-5 flex flex-col sm:flex-row gap-6 sm:items-start">
        <Qr w={w} />
        <div className="min-w-0 space-y-4">
          <p className="font-mono text-[0.9375rem] break-all select-all bg-inset p-3 border border-rule">{w.address}</p>
          <CopyButton text={w.address} />
        </div>
      </div>
      <p className="mt-6 font-semibold text-stamp">
        Send only {w.asset} on the {w.network} network ({w.standard}). Any other coin, or {w.asset} on another network, sent to this
        address is lost for good. We cannot get it back or refund it.
      </p>
      <p className="mt-3 text-[0.9375rem] muted">
        Before you send, check that the address matches{" "}
        <a href={blob("website/content/support.json")} className="link">
          website/content/support.json
        </a>{" "}
        in the repository. A copy of this page could carry someone else's address. Payments to it are public:{" "}
        <a href={w.explorer} className="link">
          {w.explorerName}
        </a>{" "}
        lists every one.
      </p>
    </article>
  );
}

const Support = () => {
  const { costs } = support;
  return (
    <>
      <PageHeader kicker="Support" title="Help pay for the tools">
        <p>
          Life Framework is free and stays free. Building it costs money every month: the AI subscription the code is written with, the
          website, the domain, and later a game server. Donations pay those bills.
        </p>
      </PageHeader>

      {!supportOpen && (
        <div className="wrap">
          <div className="sheet p-5 md:p-8 grid gap-6 md:grid-cols-[auto_1fr] md:items-center">
            <Stamp tilt>Not open yet</Stamp>
            <p className="muted">
              We do not take donations yet; the accounts are not set up. Until they are, this page lists what the project costs. When
              they are, the ways to give appear here.
            </p>
          </div>
        </div>
      )}

      {supportOpen && (
        <Section id="give" kicker="Ways to give" title="Card or crypto">
          <div className="grid gap-8 lg:grid-cols-2 lg:items-start">
            {support.card && (
              <article className="sheet p-6 md:p-8">
                <p className="kicker">Card</p>
                <h3 className="display-3 mt-1">{support.card.name}</h3>
                <p className="mt-3 muted">
                  Pay by card, once or monthly, through {support.card.name}. They handle the payment; we never see your card details.
                </p>
                <p className="mt-6">
                  <a href={support.card.url} className="btn btn-primary">
                    Give through {support.card.name}
                  </a>
                </p>
              </article>
            )}
            {support.crypto.map((w) => (
              <WalletSheet key={w.address} w={w} />
            ))}
          </div>
        </Section>
      )}

      <Section id="costs" kicker="Transparency" title="Where the money goes">
        <ul className="border-t-2 border-ink max-w-4xl">
          {costs.items.map((c) => (
            <li key={c.what} className="py-4 border-b border-rule grid gap-x-6 gap-y-1 sm:grid-cols-[1fr_auto]">
              <span>
                <span className="heading block">{c.what}</span>
                <span className="block muted text-[0.9375rem] mt-1">{c.why}</span>
              </span>
              <span className="font-mono text-[0.875rem] sm:text-right sm:pt-1">{amountText(c)}</span>
            </li>
          ))}
        </ul>
        <p className="mt-4 font-mono text-[0.75rem] uppercase tracking-[0.06em] text-ink-2">
          Updated {formatDate(costs.updated)} ·{" "}
          <a href={blob("website/content/costs.json")} className="link">
            costs.json and its history
          </a>
        </p>
      </Section>

      <Section id="what-you-get" kicker="What a donation buys" title="Our thanks, nothing in the game">
        <div className="grid gap-8 md:grid-cols-2 max-w-5xl">
          <p className="muted">
            A donation buys no money, gear, vehicles, property, queue priority, faster whitelist, or role, on our server or anyone
            else's. The framework is never sold. Bohemia forbids paid gameplay advantages on Reforger servers, and we would not sell
            them anyway.
          </p>
          <ul className="space-y-2">
            <li>
              <GoLink to="/decisions#the-project-takes-donations-to-pay-its-running-costs">The decision on donations</GoLink>
            </li>
            <li>
              <GoLink to="/decisions#the-framework-is-always-free-the-flagship-may-take-cosmetic-donations">The flagship's cosmetic rule</GoLink>
            </li>
            <li>
              <GoLink to={blob("docs/community/support.md")}>How this is set up</GoLink>
            </li>
          </ul>
        </div>
        <p className="mt-10 max-w-prose muted">
          Money is one way to help. Playing a playtest, filing a bug, or taking a scope helps as much.{" "}
          <Link to="/contribute" className="link">
            How to contribute
          </Link>
          .
        </p>
      </Section>
    </>
  );
};

export default Support;
