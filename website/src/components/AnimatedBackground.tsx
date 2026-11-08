export const AnimatedBackground = () => {
  return (
    <div className="fixed inset-0 -z-10 overflow-hidden pointer-events-none">
      {/* Gradient overlay */}
      <div className="absolute inset-0 bg-gradient-to-br from-background via-primary/5 to-accent/10" />
      
      {/* Animated grid - more visible */}
      <div 
        className="absolute inset-0 opacity-40"
        style={{
          backgroundImage: `
            linear-gradient(to right, hsl(var(--primary) / 0.3) 1px, transparent 1px),
            linear-gradient(to bottom, hsl(var(--primary) / 0.3) 1px, transparent 1px)
          `,
          backgroundSize: '50px 50px',
          animation: 'grid-move 15s linear infinite',
        }}
      />
      
      {/* Secondary diagonal grid */}
      <div 
        className="absolute inset-0 opacity-20"
        style={{
          backgroundImage: `
            linear-gradient(45deg, hsl(var(--accent) / 0.2) 1px, transparent 1px),
            linear-gradient(-45deg, hsl(var(--accent) / 0.2) 1px, transparent 1px)
          `,
          backgroundSize: '70px 70px',
          animation: 'grid-move 25s linear infinite reverse',
        }}
      />
      
      {/* Animated glow orbs - more prominent */}
      <div className="absolute top-0 -left-1/4 w-3/4 h-3/4 bg-primary/20 rounded-full blur-[150px] animate-pulse" 
           style={{ animationDuration: '6s' }} />
      <div className="absolute bottom-0 -right-1/4 w-3/4 h-3/4 bg-accent/20 rounded-full blur-[150px] animate-pulse" 
           style={{ animationDuration: '8s', animationDelay: '2s' }} />
      <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-1/2 h-1/2 bg-primary/15 rounded-full blur-[100px] animate-pulse" 
           style={{ animationDuration: '10s', animationDelay: '1s' }} />
    </div>
  );
};
