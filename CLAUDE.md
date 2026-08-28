# Claude Code cloud review instructions

These repository instructions supplement, and do not replace, the canonical project documents.

- Review a concrete pull request at documented base and HEAD SHAs. During the cloud phase, GitHub is the technical source of truth; unpushed local state is unavailable.
- Claude Code is the independent architecture, quality, and gate reviewer. Do not push review changes unless the Product Owner explicitly requests implementation.
- Codex and Claude Code must not edit the same branch concurrently.
- Read `docs/status/PROJECT_STATE.md`, `docs/process/AI_COLLABORATION.md`, the current stage brief/report, relevant accepted ADRs, and `docs/process/QUALITY_POLICY.md`.
- Inspect the full PR diff, adjacent affected failure paths, and the exact GitHub Actions jobs and logs. Windows CI is automated software evidence, not hardware evidence.
- Do not claim visual Windows GUI validation or real MIDI/SysEx/hardware validation from offscreen, fake-transport, or hosted-runner tests. Report these as pending after 09.09.2026 until the Product Owner performs them.
- Treat local absolute paths and machine-only dependencies as cloud-reproducibility risks, never prerequisites.
- Do not merge, change repository settings, or issue a stage gate unless the review request explicitly asks for that gate.
