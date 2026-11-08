import type { MouseEvent } from "react";
import { useNavigate } from "react-router-dom";

// HTML rendered from markdown at build time. Clicks on site-relative links go through the
// router, so moving between docs does not reload the page.
export function HtmlBody({ html, className = "" }: { html: string; className?: string }) {
  const navigate = useNavigate();
  const onClick = (e: MouseEvent<HTMLDivElement>) => {
    if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    const a = (e.target as HTMLElement).closest("a");
    const href = a?.getAttribute("href");
    if (!href || !href.startsWith("/") || href.startsWith("//") || a?.target) return;
    e.preventDefault();
    navigate(href);
  };
  // Delegation only: the targets are real <a> elements, which keep their keyboard behaviour.
  return <div className={className} onClick={onClick} dangerouslySetInnerHTML={{ __html: html }} />;
}
