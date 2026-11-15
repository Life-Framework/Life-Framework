// Fixture: a page that shows the next session and nothing live. This comment says "no player count" and "a2s" and must not trip the check.
const sessionFiles = import.meta.glob("../../content/sessions.json", { eager: true });
export default function Page() {
  const rows = Object.values(sessionFiles);
  return <p>Most of it has had few players. The next session is {rows.length ? "announced" : "not announced"}. Port 17777 is the A2S query.</p>;
}
