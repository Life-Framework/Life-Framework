// Fixture: a page with a player count from a live source, no fetch of its own.
import { useServerStatus } from "@/lib/live";
export default function Page() {
  const status = useServerStatus();
  return <p>{status.players} players online</p>;
}
