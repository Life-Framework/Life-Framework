import type { Config } from "tailwindcss";

// Colours and type come from the tokens in src/index.css (see BRAND.md). Use these names,
// not raw colours, so light and dark stay in step.
const token = (name: string) => `var(--${name})`;

export default {
  darkMode: ["selector", '[data-theme="dark"]'],
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    screens: { sm: "640px", md: "768px", lg: "1024px", xl: "1200px" },
    extend: {
      colors: {
        paper: token("paper"),
        card: token("card"),
        inset: token("inset"),
        ink: token("ink"),
        "ink-2": token("ink-2"),
        rule: token("rule"),
        green: token("green"),
        "on-green": token("on-green"),
        stamp: token("stamp"),
        sign: token("sign"),
        "on-sign": token("on-sign"),
      },
      fontFamily: {
        display: ["var(--font-display)"],
        sans: ["var(--font-body)"],
        mono: ["var(--font-mono)"],
      },
      maxWidth: { page: "74rem", prose: "42rem" },
    },
  },
  plugins: [],
} satisfies Config;
