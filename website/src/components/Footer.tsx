import { Link } from "react-router-dom";
import { DISCORD_URL, REPO_URL } from "@/lib/content";

const Footer = () => (
  <footer className="relative z-10 border-t border-border bg-background/80 backdrop-blur-sm">
    <div className="container mx-auto px-4 py-8 flex flex-col md:flex-row items-center justify-between gap-4 text-sm text-muted-foreground">
      <p>
        <span className="font-display font-bold text-foreground">
          <span className="text-primary">LIFE</span> FRAMEWORK
        </span>{" "}
        · MIT licensed · built from{" "}
        <a href={REPO_URL} className="text-primary hover:underline" target="_blank" rel="noopener noreferrer">
          the repository
        </a>
      </p>
      <div className="flex gap-6">
        <Link to="/documentation" className="hover:text-foreground">Docs</Link>
        <a href={REPO_URL} target="_blank" rel="noopener noreferrer" className="hover:text-foreground">GitHub</a>
        <a href={DISCORD_URL} target="_blank" rel="noopener noreferrer" className="hover:text-foreground">Discord</a>
      </div>
    </div>
  </footer>
);

export default Footer;
