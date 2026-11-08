# Support and donations

How Life Framework takes money to pay its running costs, and how to switch it
on. Status on 2026-11-08: **off.** No account or wallet exists yet. The site's
`/support` page lists the costs and says donations are not open.

The rules come from `docs/decisions.md`: the framework is never sold, a
donation buys nothing in any game, and the flagship server may later take
cosmetic-only donations under Bohemia's server policy. Facts below were read
from the linked pages on 2026-11-08. Anything marked UNVERIFIED could not be
read from a primary source (the page blocked automated reads or said nothing);
check it before relying on it.

## The choice: card routes, ranked by what becomes public

Every card payment goes through a processor that must verify someone's
identity (KYC). There is no card route without that. The question is whose
identity, who holds it, and what is shown in public. Ranked from least to most
public exposure of the maintainer:

| Route | Shown in public | Held privately | Card, Apple Pay, Google Pay | Fees | Payout | Donor needs an account | Setup |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **USDT to a self-custody address** (no card) | The address and every payment to it, on the chain explorer | Nothing; no account exists | No card. Donor must already hold USDT | Sender pays the network fee; no platform fee | Already in your wallet. Moving it out needs TRX for fees | A crypto wallet | 15 to 30 minutes |
| **Open Source Collective** (fiscal host) | The collective, its admins' profiles, every donation and every expense with its payee profile | The host holds the money and does payout checks | Card through Stripe. Apple and Google Pay UNVERIFIED | 10% host fee plus card processing | You file an expense with a receipt; the host reimburses it | No (guest checkout, UNVERIFIED) | Apply, then wait for acceptance (time UNVERIFIED) |
| **GitHub Sponsors** (org `Life-Framework`) | The org's sponsor page. Whether the payee's legal name shows: UNVERIFIED (we found no sign that it does) | Stripe holds your KYC and bank details; GitHub holds the US tax form | Card through GitHub billing. Apple and Google Pay UNVERIFIED | 0% from personal accounts; up to 6% from organisation accounts | Monthly to your bank via Stripe Connect, or to a fiscal host | **Yes, a GitHub account.** Most players do not have one | About an hour, then GitHub review of a few days |
| **Liberapay** | Your Liberapay profile name (can be a project name) | Your own Stripe or PayPal account | Card and SEPA through Stripe. Apple and Google Pay UNVERIFIED | No platform fee; Stripe about 3%, PayPal about 5% | Straight into your Stripe or PayPal | Yes | About an hour. Recurring only; one-off gifts are not supported |
| **Ko-fi** | The page name | Your own Stripe or PayPal account. PayPal shows your account name to donors; Stripe card statements show your business name or legal name unless you set one | Card through Stripe or PayPal | Free plan 5% (0% on one-off tips per third parties, UNVERIFIED); Gold $12 a month for 0%; Stripe or PayPal fees on top | Straight into Stripe or PayPal | No | Under an hour |
| **Buy Me a Coffee** | The page name | Stripe (same statement-name issue as Ko-fi) | Card through Stripe | 5% plus Stripe 2.9% + $0.30, more for international cards | Stripe; minimum $10 payout (secondary source) | No | Under an hour |
| **Card into the USDT address** (MoonPay, Transak, Coinbase Onramp, Ramp) | The address | The donor does KYC with the on-ramp. **We need a partner account**: MoonPay and Coinbase require a signed link from a server we run; Transak requires an API key | Card, often Apple Pay, through the on-ramp | Donor pays the on-ramp's fee (amount UNVERIFIED) | Lands in the wallet | Donor KYC with the on-ramp | Business onboarding with the provider plus a small backend. Not low-maintenance |
| **Patreon** | The creator page | Patreon and its processor | Card | 10% for pages published after 4 August 2025, plus processing | Patreon payout | Yes | Under an hour. Built for creator rewards, not cost-sharing |

What this means:

- **Least exposure, no card:** the self-custody USDT address. No account, no
  KYC to receive. It stays pseudonymous until you cash it out through an
  exchange, which will ask for KYC.
- **Least public exposure with cards:** Open Source Collective. The host holds
  the money and you claim costs with receipts, but the ledger is public by
  design and costs 10%. Your payee profile appears on each expense; use a
  profile name you are comfortable showing (whether a pseudonymous profile is
  accepted for payouts is UNVERIFIED).
- **Cheapest with cards:** GitHub Sponsors. No fee for personal-account
  sponsors and your identity stays with Stripe and GitHub. The cost is that
  every donor needs a GitHub account, which shuts out most players.
- **Easiest for players with cards:** Ko-fi. No donor account, cards and
  PayPal. Use Stripe, not PayPal, and set a Stripe business name (for example
  "Life Framework") so donors' statements do not show your legal name.
- **Not worth it now:** card on-ramps into the wallet (partner onboarding and a
  backend for a few donations), Patreon (10% and reward-shaped), Buy Me a
  Coffee (Ko-fi does the same for less).

## Recommendation

1. **Primary: a self-custody USDT address on TRON (TRC-20).** It is what the
   maintainer asked for: no account, no KYC to receive, near-zero upkeep, and
   every payment is publicly checkable on Tronscan, which is the transparency
   page for free.
2. **Secondary, for cards: the maintainer picks one from the table.** Our lean
   is **Ko-fi via Stripe with a business name set** if reaching players
   matters most, or **Open Source Collective** if keeping the money out of
   your own name and showing a public ledger matters more than its 10%. GitHub
   Sponsors is the cheapest but needs donors to have GitHub accounts.
3. **No Patreon, no Buy Me a Coffee, no card on-ramp** for now.

## Bohemia's rules

Two pages govern money around Arma Reforger. Neither mentions donations to a
mod project.

- **Server monetisation policy** (2 June 2025),
  https://reforger.armaplatform.com/news/server-monetization. It applies to a
  *server*, so to the flagship later, not to the project:
  - "Accepting donations is allowed, but not providing donations must not
    prevent anyone from accessing any content."
  - Charging for access is allowed "if the fees and associated perks do not
    affect gameplay in any way". Selling gameplay items (its example: "a ghillie
    suit, weapons, attachments") is forbidden.
  - A monetised server must apply, be approved, and appear on Bohemia's
    approved list; it stops monetising if removed. The application lives at
    https://www.bohemia.net/monetization (content UNVERIFIED; the page blocked
    automated reads).
- **Workshop Terms of Use**, section 3,
  https://reforger.armaplatform.com/workshop-terms. A custom licence "shall
  explicitly prohibit any direct or indirect monetization of the Content".

So we keep two things apart:

- **Donations to the project** pay the project's costs (AI tooling, website,
  domain). They are not tied to the mod: the mod, its Workshop page, and its UI
  carry no donation link, and donating unlocks nothing. This is our reading of
  the rules above, not a Bohemia statement; ask Bohemia in writing before
  linking donations from anywhere near the Workshop.
- **Flagship server perks** (M4 at the earliest) need Bohemia's approval first,
  stay cosmetic, and never gate content.

## Where the money goes

`website/content/costs.json` is the public cost list on `/support`. The
maintainer edits it; the page shows `null` as "Not filled in yet". Known today:

| Cost | Amount | Source |
| --- | --- | --- |
| Website hosting | Free | Cloudflare Pages free plan |
| Domain `life-framework.org` | TODO. Cloudflare Registrar renews a .org at cost, about $10 a year | Fill in what the registrar charges |
| Claude subscription | TODO | Fill in the plan you pay for |
| Game server | Not yet | Playtests run on the maintainer's PC |

Never type an estimate into `amount`. Change `updated` when you edit the file.

## What is wired, and how to switch it on

| Piece | Where | Off state (now) |
| --- | --- | --- |
| Support page | `website/src/pages/Support.tsx`, route `/support` | Lists costs, a NOT OPEN YET stamp, no ways to give |
| Ways to give | `website/content/support.json` | `"enabled": false`, every address and link empty |
| Footer link | `website/src/components/Layout.tsx` | Hidden |
| GitHub Sponsor button | `.github/FUNDING.yml` | File does not exist |
| README | `README.md`, "Support" | Says donations are not open yet |
| Discord | #welcome | Nothing posted |

The page draws each address's QR code at build time (`website/scripts/support.mjs`,
`qrcode-generator`); no QR web service sees it. The build refuses to publish:

- a TRON address whose checksum fails, or an Ethereum address of the wrong shape;
- a card link that is not https, or a card link without a name;
- `"enabled": true` with nothing filled in;
- `"enabled": true` without `.github/FUNDING.yml`, or the file without `"enabled": true`.

### The switch (one PR)

1. In `website/content/support.json`, paste the address into the TRC-20 entry,
   and/or fill `card.name` and `card.url`. Delete the ERC-20 entry unless you
   made one. Set `"enabled": true`. That line is the switch.
2. Add `.github/FUNDING.yml`:

   ```yaml
   # Only the lines for accounts that exist.
   # github: [Life-Framework]      # if GitHub Sponsors is the card route
   # ko_fi: <ko-fi username>       # if Ko-fi is the card route
   # open_collective: <slug>       # if Open Source Collective is the card route
   # liberapay: <username>         # if Liberapay is the card route
   custom: ["https://life-framework-arn.pages.dev/support"]
   ```

   Change the `custom` URL to `https://life-framework.org/support` at the DNS
   cutover (`website/README.md`).
3. In `README.md`, replace "We do not take donations yet. When we do, the
   site's support page lists the ways to give and every cost." with "The ways
   to give and every cost are on the [support page](https://life-framework-arn.pages.dev/support)."
4. `cd website; npm run build; npm run check`. Open `/support` and compare the
   address on the page and in the QR code with the one in your wallet.

To switch off: `"enabled": false` and delete `.github/FUNDING.yml`.

## Maintainer setup

You do these; no agent can, and no agent should ever see a secret. What we
need back is public: an address or a URL.

### A. Self-custody USDT wallet (TRON, TRC-20)

About 15 to 30 minutes.

1. **Pick a wallet that keeps the keys on your device.** For more than pocket
   money, a hardware wallet that supports TRON (Trezor and Ledger both do). For
   a start, a non-custodial app such as TronLink. Do not use an exchange
   account: it is custodial and requires KYC.
2. **Create the wallet and write the recovery phrase on paper.** Keep it
   offline. Never type it into a website, a chat, a password manager you
   share, a GitHub issue, Discord, or any AI agent, including ours. Nobody from
   the project will ever ask for it. Anyone with the phrase owns the money.
3. **Open the TRON account and copy its receive address.** It starts with `T`
   and is 34 characters. Choose TRC-20 because fees are low. An Ethereum
   (ERC-20) address is optional and costs donors more to use; skip it unless
   someone asks.
4. **Keep a little TRX in the wallet.** Receiving USDT is free, but sending it
   on later burns TRX (or uses staked energy) as the fee.
5. **Test it.** Send a small amount of USDT on TRC-20 to the address from
   anywhere, and see it arrive on https://tronscan.org.
6. **Send us the address only.** Post it in the PR or tell the lead. We put it
   in `website/content/support.json`; you check the merged page against your
   wallet before anyone is told.

Things to know:

- Every payment to the address is public and permanent on the chain. Donors
  and amounts are visible to anyone with the address. That is the
  transparency, and also the privacy limit: once you pay someone from it, they
  can follow the trail.
- Turning USDT into euros or dollars on an exchange almost always needs KYC
  there.
- Tax may still apply where you live (income, or wealth held in crypto). We
  did not check the rules for your country; ask your tax office or an
  accountant.
- A lost recovery phrase means lost money. A crypto payment cannot be
  refunded or reversed.

### B. One card route (only the one you pick)

**Ko-fi** (under an hour)

1. Sign up at https://ko-fi.com with a project name ("Life Framework"), not
   your own.
2. Connect **Stripe** for cards (Stripe asks for ID, address, and bank
   account; it is private to Stripe). Skip PayPal: it shows your PayPal name
   to donors.
3. In the Stripe dashboard, set the public business name and statement
   descriptor to "LIFE FRAMEWORK" so card statements do not show your legal
   name.
4. Turn off shop, commissions, and memberships. Donations only.
5. Send us `https://ko-fi.com/<name>`.

**Open Source Collective** (form in under an hour, acceptance takes longer)

1. Go to https://opencollective.com/create, sign in with GitHub, and pick Open
   Source Collective as the host. It checks an open-source licence, an
   organisation repo, and recent activity
   (https://docs.oscollective.org/getting-started/acceptance-criteria); we meet
   all three. Two admins are preferred: add the lead.
2. Use "Life Framework" as the collective name and the GitHub repo URL.
3. After acceptance, set the contribution options to one-off and monthly with
   free amounts.
4. To spend: submit an expense with the receipt (Claude invoice, domain
   renewal). It is paid to the payout method on your profile.
5. Send us `https://opencollective.com/<slug>`.

**GitHub Sponsors** (about an hour, then a few days of review)

1. An owner of the `Life-Framework` org turns on two-factor authentication and
   opens https://github.com/sponsors/accounts, then "Get sponsored" next to
   `Life-Framework`.
2. Choose bank account (Stripe Connect; ID, address, bank, private to Stripe)
   or a fiscal host. Switching later goes through GitHub Support.
3. Fill in the US tax form GitHub asks for (W-8BEN as an individual outside
   the US; W-8BEN-E as a company).
4. Profile: paste the short text below. Tiers: one-time with a custom amount,
   and monthly at $5, $10, and $25, each rewarded with thanks only.
5. Submit for review (https://docs.github.com/en/sponsors/receiving-sponsorships-through-github-sponsors/setting-up-github-sponsors-for-your-organization).
6. Send us `https://github.com/sponsors/Life-Framework`.

Profile text for any platform:

> Life Framework is an open-source life and roleplay framework for Arma
> Reforger, and the 1989 Everon server that proves it. Money given here pays
> the project's running costs: the Claude subscription the AI-first
> development runs on, the website and domain, and later a game server. The
> costs are listed at https://life-framework-arn.pages.dev/support. A donation
> buys nothing in any game. Thank you.

## Discord (apply only after the switch)

The maintainer wants Discord quiet, so no announcement and no new channel.
Once `/support` shows a way to give, the bot account edits its existing
#welcome message to add this as the last paragraph. An edit sends no
notification.

> **Support.** Life Framework is free. If you want to help pay for its running
> costs (AI tooling, the website, later a server), the ways to give and every
> cost are at https://life-framework.org/support. A donation buys nothing in
> the game.

Use the pages.dev address until the DNS cutover. After applying it, add a line
to the #welcome row in `docs/community/discord.md`.

## Sources

- GitHub Sponsors fees: https://docs.github.com/en/enterprise-cloud@latest/sponsors/sponsoring-open-source-contributors/about-sponsorships-fees-and-taxes
- GitHub Sponsors regions and org setup: https://docs.github.com/en/sponsors/getting-started-with-github-sponsors/about-github-sponsors
- Open Source Collective fees: https://docs.oscollective.org/how-it-works/fees
- Open Collective platform moved to the Open Finance Consortium, 1 October 2024: https://opencollective.com/ofico/updates/a-new-home-for-the-open-collective-platform-same-mission
- Open Collective incognito covers contributors, not admins: https://opencollective.com/opencollective/updates/removing-identifying-information-from-incognito-contributions
- Liberapay fees and processors: https://liberapay.com/about/faq, https://liberapay.com/about/payment-processors
- Ko-fi Gold: https://ko-fi.com/gold
- Buy Me a Coffee fees: https://help.buymeacoffee.com/en/articles/8105744-how-to-calculate-charges-on-your-payment
- Patreon 10% plan: https://support.patreon.com/hc/en-us/articles/36426991446797
- Stripe statement descriptor: https://support.stripe.com/questions/what-is-a-statement-descriptor-and-how-to-update-it
- On-ramp partner keys: https://dev.moonpay.com/widget/on-ramp/customization/url-signing.md, https://docs.transak.com/customization/query-parameters.md, https://docs.cdp.coinbase.com/onramp-&-offramp/secure-init-migration
- TRC-20 fees on send: https://trezor.io/guides/sending-receiving-staking-funds/sending-receiving/send-and-receive-tron-and-trc-20-tokens-in-trezor-suite
