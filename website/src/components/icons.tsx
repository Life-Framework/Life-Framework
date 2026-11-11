// The few marks the site uses. Brand logos only where they name the service.
type P = { className?: string };

// The small mark from public/brand (BRAND.md "Logo"), one file per theme.
export const Mark = ({ className = "w-8 h-8" }: P) => (
  <>
    <img src="/brand/mark-light.svg" alt="" width={32} height={32} className={`${className} dark:hidden`} />
    <img src="/brand/mark-dark.svg" alt="" width={32} height={32} className={`${className} hidden dark:block`} />
  </>
);

export const GitHubIcon = ({ className = "w-5 h-5" }: P) => (
  <svg viewBox="0 0 24 24" className={className} fill="currentColor" aria-hidden="true" focusable="false">
    <path d="M12 .5a11.5 11.5 0 0 0-3.64 22.41c.58.1.79-.25.79-.56v-2c-3.2.7-3.88-1.37-3.88-1.37-.53-1.33-1.28-1.69-1.28-1.69-1.05-.71.08-.7.08-.7 1.16.08 1.77 1.19 1.77 1.19 1.03 1.77 2.7 1.26 3.36.96.1-.75.4-1.26.73-1.55-2.56-.29-5.25-1.28-5.25-5.68 0-1.26.45-2.28 1.19-3.09-.12-.29-.52-1.46.11-3.05 0 0 .97-.31 3.17 1.18a11 11 0 0 1 5.77 0c2.2-1.49 3.17-1.18 3.17-1.18.63 1.59.23 2.76.11 3.05.74.81 1.19 1.83 1.19 3.09 0 4.41-2.69 5.38-5.26 5.67.41.36.78 1.06.78 2.14v3.17c0 .31.21.67.8.56A11.5 11.5 0 0 0 12 .5Z" />
  </svg>
);

export const DiscordIcon = ({ className = "w-5 h-5" }: P) => (
  <svg viewBox="0 0 24 24" className={className} fill="currentColor" aria-hidden="true" focusable="false">
    <path d="M20.32 4.37A19.8 19.8 0 0 0 15.43 2.85a13.8 13.8 0 0 0-.62 1.25 18.3 18.3 0 0 0-5.49 0 12.6 12.6 0 0 0-.62-1.25 19.7 19.7 0 0 0-4.88 1.52C.53 9.05-.32 13.58.1 18.06a19.9 19.9 0 0 0 6 3.03c.49-.66.92-1.36 1.29-2.1-.71-.27-1.38-.6-2.02-.98l.5-.39a14.2 14.2 0 0 0 12.27 0l.5.39c-.64.38-1.32.71-2.03.98.37.74.8 1.44 1.29 2.1a19.8 19.8 0 0 0 6-3.03c.5-5.18-.84-9.67-3.58-13.69ZM8.02 15.33c-1.18 0-2.16-1.08-2.16-2.42 0-1.33.96-2.42 2.16-2.42 1.21 0 2.18 1.1 2.16 2.42 0 1.34-.96 2.42-2.16 2.42Zm7.97 0c-1.18 0-2.15-1.08-2.15-2.42 0-1.33.95-2.42 2.15-2.42 1.21 0 2.18 1.1 2.16 2.42 0 1.34-.95 2.42-2.16 2.42Z" />
  </svg>
);

export const Arrow = ({ className = "w-4 h-4" }: P) => (
  <svg viewBox="0 0 16 16" className={className} fill="none" stroke="currentColor" strokeWidth="2" aria-hidden="true" focusable="false">
    <path d="M2 8h11M9 4l4 4-4 4" />
  </svg>
);

export const LinkIcon = ({ className = "w-4 h-4" }: P) => (
  <svg viewBox="0 0 16 16" className={className} fill="none" stroke="currentColor" strokeWidth="1.75" aria-hidden="true" focusable="false">
    <path d="M6.5 9.5a2.5 2.5 0 0 0 3.5 0l2.5-2.5a2.5 2.5 0 0 0-3.5-3.5L7.5 5M9.5 6.5a2.5 2.5 0 0 0-3.5 0L3.5 9a2.5 2.5 0 0 0 3.5 3.5l1-1" />
  </svg>
);

export const External = ({ className = "w-3.5 h-3.5 inline-block align-[-0.1em]" }: P) => (
  <svg viewBox="0 0 16 16" className={className} fill="none" stroke="currentColor" strokeWidth="1.75" aria-hidden="true" focusable="false">
    <path d="M6 3H3v10h10v-3M9 3h4v4M13 3 7 9" />
  </svg>
);

export const DownloadIcon = ({ className = "w-5 h-5" }: P) => (
  <svg viewBox="0 0 16 16" className={className} fill="none" stroke="currentColor" strokeWidth="2" aria-hidden="true" focusable="false">
    <path d="M8 2v8M4.5 6.5 8 10l3.5-3.5M2.5 13.5h11" />
  </svg>
);
