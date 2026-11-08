import { useEffect, useRef } from "react";
import { Navigate, Route, Routes, useLocation } from "react-router-dom";
import { Layout } from "@/components/Layout";
import { fullTitle, metaFor } from "@/lib/meta";
import Home from "@/pages/Home";
import Roadmap from "@/pages/Roadmap";
import Devlog from "@/pages/Devlog";
import Servers from "@/pages/Servers";
import Contribute from "@/pages/Contribute";
import Decisions from "@/pages/Decisions";
import Support from "@/pages/Support";
import { DocView, DocsIndex } from "@/pages/Docs";
import NotFound from "@/pages/NotFound";

// After a client-side navigation: scroll to the hash or the top, set the title, and move
// focus to the page so a screen reader announces it. The first load is left alone.
function RouteEffects() {
  const { pathname, hash } = useLocation();
  const first = useRef(true);
  useEffect(() => {
    document.title = fullTitle(metaFor(pathname));
    if (first.current) {
      first.current = false;
      return;
    }
    const target = hash ? document.getElementById(decodeURIComponent(hash.slice(1))) : null;
    if (target) target.scrollIntoView();
    else {
      window.scrollTo(0, 0);
      document.getElementById("main")?.focus({ preventScroll: true });
    }
  }, [pathname, hash]);
  return null;
}

const App = () => (
  <Layout>
    <RouteEffects />
    <Routes>
      <Route path="/" element={<Home />} />
      <Route path="/roadmap" element={<Roadmap />} />
      <Route path="/devlog" element={<Devlog />} />
      <Route path="/servers" element={<Servers />} />
      <Route path="/contribute" element={<Contribute />} />
      <Route path="/decisions" element={<Decisions />} />
      <Route path="/support" element={<Support />} />
      <Route path="/docs" element={<DocsIndex />} />
      <Route path="/docs/decisions" element={<Navigate to="/decisions" replace />} />
      <Route path="/docs/:slug" element={<DocView />} />
      <Route path="/documentation" element={<Navigate to="/docs" replace />} />
      <Route path="/philosophy" element={<Navigate to="/" replace />} />
      <Route path="/team" element={<Navigate to="/contribute" replace />} />
      <Route path="*" element={<NotFound />} />
    </Routes>
  </Layout>
);

export default App;
