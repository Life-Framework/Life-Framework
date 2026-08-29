# Contributing to Life Framework

First off, thank you for considering contributing to Life Framework! It's people like you that make this community-driven project possible.

## Our Philosophy

We're a group of passionate modders who love life mods and want to bring back the vibrant community we all enjoyed. This is a **passion project** – we're here because it's fun, and we want to share that fun with everyone.

### We Welcome Everyone

Whether you're an experienced developer or just getting started, we want your input, your ideas, and your code. Everyone has something valuable to contribute, and we're committed to making this a welcoming space for all skill levels.

## Community & Communication

### 💬 Discord is Our Home

We primarily use **Discord** for all community communication and collaboration. This is where the magic happens!

**[Join our Discord server](https://discord.com/invite/MjMexJteqz)** to:
- **Report bugs** in dedicated forums
- **Suggest features** and discuss ideas
- **Get help** from the community and maintainers
- **Share your work** and show off what you've built
- **Chat with other modders** in real-time
- **Stay updated** on the latest developments

Our Discord is organized with:
- **Forums** for structured discussions (bug reports, feature requests, development topics)
- **Text chats** for casual conversation, quick questions, and community hangouts
- **Voice channels** for collaborative sessions and community events

While we use GitHub for code and pull requests, Discord is where we build relationships, share knowledge, and collaborate as a community.

## How Can I Contribute?

### 🐛 Reporting Bugs

Found a bug? Help us squash it!

1. **[Join our Discord server](https://discord.com/invite/MjMexJteqz)** and check the bug reports forum
2. **Search existing reports** to see if it's already been reported
3. **Post in the bug reports forum** with a clear title and description
4. **Include details:**
   - Steps to reproduce the bug
   - Expected vs. actual behavior
   - Your environment (Arma Reforger version, OS, etc.)
   - Screenshots or logs if applicable

### 💡 Suggesting Features

Have an idea for a new feature or improvement?

1. **[Join our Discord server](https://discord.com/invite/MjMexJteqz)** and check the feature suggestions forum
2. **Search existing suggestions** to see if it's already been proposed
3. **Post your idea** in the feature suggestions forum
4. **Explain the use case** – how would this benefit the community?
5. **Be open to feedback** – we'll discuss and refine ideas together in the thread

### 🔧 Code Contributions

Ready to write some code? Awesome!

#### Getting Started

1. **Fork the repository** to your GitHub account
2. **Clone your fork** locally
3. **Create a branch** for your feature or fix:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. **Make your changes** following our coding standards
5. **Test thoroughly** – make sure everything works!
6. **Commit your changes** with clear, descriptive messages
7. **Push to your fork** and submit a pull request

#### Coding Standards

- **Write clean, readable code** – others will be reading and maintaining it
- **Comment complex logic** – help future contributors understand your thinking
- **Follow existing patterns** – consistency makes the codebase easier to navigate
- **Test your changes** – ensure they work in-game before submitting

#### Pull Request Process

GitHub pre-fills new PRs from [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md). Use it — it matches our verification checklist.

1. **Describe your changes** clearly in the PR description (Summary, Verification, Manual testing)
2. **Reference related issues** if applicable (e.g., "Fixes #123")
3. **Check off verification** — `validate`, the right `test` tier, red-proof tests if applicable
4. **Be responsive** to feedback and review comments
5. **Keep PRs focused** — one feature or fix per PR when possible
6. **Update documentation** if your changes affect how things work

### 🤖 AI-assisted development

We **encourage** AI-assisted contributions. Cursor, Claude Code, Codex, opencode, and similar tools are welcome — the repo ships a harness so humans and agents share the same workflow.

#### The harness

| Piece | Purpose |
| --- | --- |
| [`AGENTS.md`](AGENTS.md) | Portable contract: golden rules, EnforceScript traps, data rules, verification ladder. **Read this first** — every tool can use it. |
| [`.opencode/`](.opencode/README.md) | Skills, playbooks, and agents for [opencode](https://opencode.ai) (`/life-mode`, `/prove`, `/interrogate`). Plain markdown — other assistants can read it too. |
| [`tools/cli`](tools/README.md) | Unified CLI: `validate`, `build`, `test`, `ci`, MCP install/call. |
| **MCP servers** | Enfusion API search, vanilla source, asset lookup, optional Workbench tools — `node tools/cli.mjs mcp install`, then `node tools/cli.mjs call …`. See [`tools/mcp/README.md`](tools/mcp/README.md). |
| **PR template** | Checklist aligned with the verification ladder (above). |

When a skill file and `AGENTS.md` disagree, **`AGENTS.md` wins**.

#### Quick start

```sh
node tools/cli.mjs status              # toolchain + MCP state
node tools/cli.mjs mcp install         # one-time: clone and build MCP servers
node tools/cli.mjs validate            # cheap — run often
node tools/cli.mjs test --tier fast    # logic-only changes
node tools/cli.mjs test                # full tier before opening a PR
```

On Windows, `tools\cli` is a shim for the same commands. Point the paths in `opencode.json` at your Workbench and game install; restart opencode after changing MCP settings.

Research Enfusion APIs from the terminal (no GUI required):

```sh
node tools/cli.mjs call list
node tools/cli.mjs call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
```

Use `@path/to/args.json` instead of inline JSON if your shell mangles quotes.

#### What we expect from AI-assisted PRs

- **Verify, don't vibe.** Output must pass `validate` and the appropriate `test` tier. A green run beats a confident summary.
- **Look up APIs — don't guess.** Use MCP / `tools/cli call` before inventing classes, RPC patterns, or prefab fields.
- **Follow repo rules.** Unique resource GUIDs, `EL_` class prefix, localization keys, no generated artifacts — all in `AGENTS.md`.
- **Add tests for behavior changes.** New `EL_Test_*` files need a `// red-proof:` comment and registration in `EL_TestManager.c`.
- **Document manual gaps.** UI, multiplayer, JIP, and save/restart paths are not fully covered by automation — fill in the PR template's **Manual testing** section.
- **Review the diff yourself.** You are responsible for what you submit; treat the agent as a fast junior modder, not an authority.

AI assistance is welcome. Untested AI output is not.

### 📚 Documentation

Documentation is just as important as code!

- Fix typos or unclear explanations
- Add examples and tutorials
- Improve setup instructions
- Document new features or systems

### 🎨 Asset Contributions

Artists and designers are welcome too!

- Create textures, models, or UI elements
- Design logos or promotional materials
- Improve visual consistency

## Community Guidelines

### Be Respectful

- Treat everyone with respect and kindness
- Welcome newcomers and help them get started
- Provide constructive feedback
- Assume good intentions

### Be Collaborative

- Share knowledge and learnings
- Help others solve problems
- Celebrate contributions from all community members
- Work together to make the framework better

### Move Fast, But Thoughtfully

We value rapid iteration, but not at the expense of quality:

- Ship features quickly, but test them first
- Iterate based on feedback
- Don't be afraid to experiment
- Learn from mistakes and share those learnings

## Development Workflow

### We Ship Fast

Our development philosophy emphasizes:

- **Rapid iteration** – get features out quickly and improve them
- **Community feedback** – listen to what users need
- **Continuous improvement** – always refining and enhancing
- **No bureaucracy** – minimal process, maximum productivity

### Branching Strategy

- `main` – stable, production-ready code
- `develop` – integration branch for features
- `feature/*` – individual feature branches
- `fix/*` – bug fix branches

### Testing

Automated checks (human or AI — same commands):

```sh
node tools/cli.mjs validate              # repo hygiene + pre-commit checks
node tools/cli.mjs test --tier fast      # pure EnforceScript / LOGIC tests
node tools/cli.mjs test                  # full suite (world, prefab, spawn)
node tools/cli.mjs ci                    # validate + build + test
```

The in-game `EL_Test*` suite runs on DebugWorld boot and prints `[ELTEST]` markers; `tools/cli test` parses them and exits nonzero on failure. See [`AGENTS.md`](AGENTS.md) for tiers, red-proof tests, and what still needs manual proof.

Also verify when relevant:

- In-game behavior on DebugWorld or MainWorld
- Compatibility with the base framework
- Performance and edge cases
- Multiplayer, JIP, UI, or save/restart paths (manual — document in your PR)

## Sharing Knowledge

Our commitment goes beyond just this mod. When you solve problems or discover better approaches:

- **Document your solutions** in the wiki or discussions
- **Share code patterns** that worked well
- **Write tutorials** for common tasks
- **Help others** who encounter similar challenges

## Recognition

All contributors will be recognized for their work:

- Contributors are listed in our documentation
- Significant contributions are highlighted in release notes
- We celebrate community achievements together

## Questions?

Don't hesitate to ask questions!

- **[Join our Discord](https://discord.com/invite/MjMexJteqz)** and ask in the general chat or help channels
- **Use Discord forums** for more in-depth technical discussions
- **Reach out to maintainers** on Discord if you need guidance

## License

By contributing to Life Framework, you agree that your contributions will be licensed under the MIT License, ensuring they remain free and open for everyone.

---

**Thank you for being part of the Life Framework community!** 🎉

Your contributions, no matter how big or small, help make this project better for everyone. We're excited to see what we'll build together!
