import { Link, useLocation } from "react-router-dom";
import { AnimatedBackground } from "@/components/AnimatedBackground";
import { Button } from "@/components/ui/button";

const NotFound = () => {
  const location = useLocation();
  return (
    <div className="min-h-screen pt-24 pb-16 relative flex items-center justify-center">
      <AnimatedBackground />
      <div className="text-center relative z-10 px-4">
        <h1 className="mb-4 text-5xl font-display font-bold text-primary">404</h1>
        <p className="mb-2 text-xl">Page not found</p>
        <p className="mb-6 text-sm text-muted-foreground font-mono">{location.pathname}</p>
        <Button asChild>
          <Link to="/">Return to Home</Link>
        </Button>
      </div>
    </div>
  );
};

export default NotFound;
