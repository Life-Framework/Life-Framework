import { BrowserRouter, Routes, Route, Navigate, useLocation } from "react-router-dom";
import { useEffect } from "react";
import Navigation from "./components/Navigation";
import Footer from "./components/Footer";
import Index from "./pages/Index";
import Team from "./pages/Team";
import Philosophy from "./pages/Philosophy";
import Roadmap from "./pages/Roadmap";
import Contribute from "./pages/Contribute";
import Documentation from "./pages/Documentation";
import DocumentView from "./pages/DocumentView";
import NotFound from "./pages/NotFound";

function ScrollToTop() {
  const { pathname, hash } = useLocation();
  useEffect(() => {
    if (hash) {
      const el = document.getElementById(decodeURIComponent(hash.slice(1)));
      if (el) {
        el.scrollIntoView();
        return;
      }
    }
    window.scrollTo(0, 0);
  }, [pathname, hash]);
  return null;
}

const App = () => (
  <BrowserRouter>
    <ScrollToTop />
    <Navigation />
    <Routes>
      <Route path="/" element={<Index />} />
      <Route path="/philosophy" element={<Philosophy />} />
      <Route path="/roadmap" element={<Roadmap />} />
      <Route path="/contribute" element={<Contribute />} />
      <Route path="/team" element={<Team />} />
      <Route path="/documentation" element={<Documentation />} />
      <Route path="/docs" element={<Navigate to="/documentation" replace />} />
      <Route path="/docs/:slug" element={<DocumentView />} />
      <Route path="*" element={<NotFound />} />
    </Routes>
    <Footer />
  </BrowserRouter>
);

export default App;
