// Fixture: a page that asks a status endpoint.
import { useEffect, useState } from "react";
export default function Page() {
  const [up, setUp] = useState(false);
  useEffect(() => {
    fetch("https://status.example.org/api/server").then((r) => setUp(r.ok));
  }, []);
  return <p>{up ? "online" : "offline"}</p>;
}
