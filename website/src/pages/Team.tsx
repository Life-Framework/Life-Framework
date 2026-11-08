import { Github, UserPlus } from "lucide-react";
import { Link } from "react-router-dom";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { REPO_URL, siteData } from "@/lib/content";

const Team = () => {
  const { members, roles } = siteData.team;

  return (
    <div className="min-h-screen pt-24 pb-16 relative">
      <AnimatedBackground />
      <div className="container mx-auto px-4 relative z-10">
        <div className="max-w-3xl mx-auto text-center mb-12">
          <div className="inline-block px-4 py-2 bg-primary/10 border border-primary/20 rounded-full text-sm font-medium mb-6 backdrop-blur-sm">
            🚀 Currently Recruiting
          </div>
          <h1 className="text-4xl md:text-5xl font-display font-bold mb-6">
            Join the <span className="text-primary">Founding Team</span>
          </h1>
          <p className="text-lg text-muted-foreground mb-8">
            We're building the future of Arma Reforger modding and looking for passionate people to join the founding
            team. Roles are earned by contribution: land a few pull requests and the role is yours.
          </p>
          <Button asChild size="lg" className="bg-primary hover:bg-primary/90 text-primary-foreground shadow-lg shadow-primary/20">
            <Link to="/contribute">
              <UserPlus className="w-4 h-4 mr-2" />
              Start Contributing
            </Link>
          </Button>
        </div>

        {members.length > 0 && (
          <div className="max-w-6xl mx-auto mb-12">
            <h2 className="text-2xl font-display font-bold mb-6 text-center">The Team</h2>
            <div className="grid sm:grid-cols-2 lg:grid-cols-4 gap-4">
              {members.map((m) => (
                <Card key={m.name} className="bg-card/50 backdrop-blur-sm border-border p-5">
                  <h3 className="font-display font-bold">{m.name}</h3>
                  <p className="text-primary text-sm font-semibold">{m.role}</p>
                  {m.bio && <p className="text-sm text-muted-foreground mt-2">{m.bio}</p>}
                  {m.github && (
                    <a href={`https://github.com/${m.github}`} target="_blank" rel="noopener noreferrer" className="inline-flex items-center gap-1 text-xs text-muted-foreground hover:text-primary mt-3">
                      <Github className="w-3.5 h-3.5" />
                      {m.github}
                    </a>
                  )}
                </Card>
              ))}
            </div>
          </div>
        )}

        <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-6 max-w-6xl mx-auto">
          {roles.map((position) => (
            <Card
              key={position.name}
              className="bg-card/50 backdrop-blur-sm border-border p-6 hover:border-primary/50 transition-all duration-300 group relative overflow-hidden"
            >
              <div className="absolute top-3 right-3">
                <div className="px-2 py-1 bg-green-500/10 border border-green-500/20 rounded text-xs font-semibold text-green-600">OPEN</div>
              </div>
              <div className="flex flex-col">
                <div className="w-16 h-16 bg-primary/10 rounded-lg mb-4 flex items-center justify-center border border-primary/20">
                  <span className="text-2xl font-display font-bold text-primary">{position.name.charAt(0)}</span>
                </div>
                <h3 className="text-xl font-display font-bold mb-1">{position.name}</h3>
                <p className="text-primary text-sm font-semibold mb-3">{position.role}</p>
                <p className="text-muted-foreground text-sm mb-4 flex-1">{position.bio}</p>
                <div className="flex flex-wrap gap-2 pt-3 border-t border-border/50">
                  {position.skills.map((skill) => (
                    <span key={skill} className="px-2 py-1 bg-muted/50 rounded text-xs font-medium text-muted-foreground">
                      {skill}
                    </span>
                  ))}
                </div>
              </div>
            </Card>
          ))}
        </div>

        <div className="max-w-3xl mx-auto text-center mt-12">
          <Card className="bg-card/50 backdrop-blur-sm border-primary/20 p-8">
            <h3 className="text-xl font-display font-bold mb-3">Don't see your role?</h3>
            <p className="text-muted-foreground mb-6">
              We're always looking for talented and passionate people. Introduce yourself on Discord, or edit{" "}
              <code className="text-xs">website/content/team.json</code> in a pull request once you've shipped something.
            </p>
            <Button asChild variant="outline" className="border-primary/50 hover:bg-primary/10">
              <a href={`${REPO_URL}/blob/main/website/content/team.json`} target="_blank" rel="noopener noreferrer">
                See team.json
              </a>
            </Button>
          </Card>
        </div>
      </div>
    </div>
  );
};

export default Team;
