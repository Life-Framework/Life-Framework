import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import rehypeSlug from "rehype-slug";
import { Link } from "react-router-dom";

type Props = { children: string; className?: string };

// Renders repo markdown. Links to other site pages stay in the router;
// everything else opens on GitHub or the wider web.
export const Markdown = ({ children, className }: Props) => (
  <div className={`doc-content ${className ?? ""}`}>
    <ReactMarkdown
      remarkPlugins={[remarkGfm]}
      rehypePlugins={[rehypeSlug]}
      components={{
        a: ({ href, children: c, node: _n, ...rest }) => {
          if (href && href.startsWith("/")) {
            return (
              <Link to={href} {...rest}>
                {c}
              </Link>
            );
          }
          if (href && href.startsWith("#")) {
            return (
              <a href={href} {...rest}>
                {c}
              </a>
            );
          }
          return (
            <a href={href} target="_blank" rel="noopener noreferrer" {...rest}>
              {c}
            </a>
          );
        },
        table: ({ node: _n, ...rest }) => (
          <div className="overflow-x-auto my-4">
            <table {...rest} />
          </div>
        ),
        img: ({ node: _n, alt, ...rest }) => <img alt={alt ?? ""} loading="lazy" {...rest} />,
      }}
    >
      {children}
    </ReactMarkdown>
  </div>
);
