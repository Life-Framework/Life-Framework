import { cn } from "@/lib/utils";

export const ProgressBar = ({ value, className }: { value: number; className?: string }) => (
  <div
    className={cn("h-2 w-full rounded-full bg-primary/15 overflow-hidden", className)}
    role="progressbar"
    aria-valuenow={value}
    aria-valuemin={0}
    aria-valuemax={100}
  >
    <div className="h-full bg-primary transition-all" style={{ width: `${Math.max(0, Math.min(100, value))}%` }} />
  </div>
);
