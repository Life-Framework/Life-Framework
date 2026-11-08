import { NavLink } from "@/components/NavLink";
import { Menu, X, Github, Users } from "lucide-react";
import { useState } from "react";
import { Button } from "@/components/ui/button";
import { DISCORD_URL, REPO_URL } from "@/lib/content";

const navItems = [
  { to: "/", label: "Home" },
  { to: "/roadmap", label: "Roadmap" },
  { to: "/philosophy", label: "Vision" },
  {
    to: "/documentation",
    label: "Docs",
    description: "Built from the repository on every merge: direction, decisions, the roadmap scopes, and the community guides.",
  },
  { to: "/contribute", label: "Contribute" },
  { to: "/team", label: "Team" },
];

const Navigation = () => {
  const [isOpen, setIsOpen] = useState(false);

  return (
    <nav className="fixed top-0 left-0 right-0 z-50 bg-background/95 backdrop-blur-md border-b border-border">
      <div className="container mx-auto px-4">
        <div className="flex items-center justify-between h-16">
          <NavLink to="/" className="text-xl font-display font-bold text-foreground flex items-center gap-2">
            <span className="text-primary">LIFE</span>
            <span>FRAMEWORK</span>
          </NavLink>

          <div className="hidden md:flex items-center gap-6">
            {navItems.map((item) => (
              <div key={item.to} className="relative group">
                <NavLink
                  to={item.to}
                  end={item.to === "/"}
                  className="text-sm font-medium text-muted-foreground hover:text-foreground transition-colors"
                  activeClassName="text-primary"
                >
                  {item.label}
                </NavLink>
                {item.description && (
                  <div className="absolute left-0 top-full mt-2 w-64 p-3 bg-popover border border-border rounded-md shadow-lg opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50">
                    <p className="text-xs text-muted-foreground leading-relaxed">{item.description}</p>
                  </div>
                )}
              </div>
            ))}

            <div className="flex items-center gap-3 ml-2 pl-6 border-l border-border">
              <Button
                asChild
                variant="outline"
                size="sm"
                className="bg-[#5865F2] hover:bg-[#4752C4] text-white hover:text-white border-[#5865F2] hover:border-[#4752C4]"
              >
                <a href={DISCORD_URL} target="_blank" rel="noopener noreferrer">
                  <Users className="w-4 h-4 mr-2" />
                  Join Discord
                </a>
              </Button>
              <Button asChild variant="outline" size="sm" className="gap-2">
                <a href={REPO_URL} target="_blank" rel="noopener noreferrer">
                  <Github className="w-4 h-4" />
                  GitHub
                </a>
              </Button>
            </div>
          </div>

          <Button
            variant="ghost"
            size="icon"
            className="md:hidden"
            aria-label={isOpen ? "Close menu" : "Open menu"}
            onClick={() => setIsOpen(!isOpen)}
          >
            {isOpen ? <X className="h-5 w-5" /> : <Menu className="h-5 w-5" />}
          </Button>
        </div>

        {isOpen && (
          <div className="md:hidden pb-4 space-y-3">
            {navItems.map((item) => (
              <div key={item.to}>
                <NavLink
                  to={item.to}
                  end={item.to === "/"}
                  className="block py-2 text-sm font-medium text-muted-foreground hover:text-foreground transition-colors"
                  activeClassName="text-primary"
                  onClick={() => setIsOpen(false)}
                >
                  {item.label}
                </NavLink>
              </div>
            ))}
            <div className="pt-4 border-t border-border space-y-2">
              <Button asChild variant="outline" size="sm" className="w-full bg-[#5865F2] hover:bg-[#4752C4] text-white hover:text-white border-[#5865F2]">
                <a href={DISCORD_URL} target="_blank" rel="noopener noreferrer">
                  <Users className="w-4 h-4 mr-2" />
                  Join Discord
                </a>
              </Button>
              <Button asChild variant="outline" size="sm" className="w-full gap-2">
                <a href={REPO_URL} target="_blank" rel="noopener noreferrer">
                  <Github className="w-4 h-4" />
                  Contribute on GitHub
                </a>
              </Button>
            </div>
          </div>
        )}
      </div>
    </nav>
  );
};

export default Navigation;
