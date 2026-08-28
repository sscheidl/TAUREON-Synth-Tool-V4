# Codex cloud instructions

These repository instructions supplement, and do not replace, the canonical project documents.

- During the cloud phase, the GitHub repository and the exact pull-request HEAD are the technical source of truth. Do not rely on unpushed local state.
- Make changes on a dedicated branch and deliver them through a pull request. Do not push directly to `main`, merge, or change repository settings unless the Product Owner explicitly requests it.
- Codex implements; Claude Code independently reviews. They must not edit the same branch concurrently.
- Read `docs/status/PROJECT_STATE.md`, `docs/process/AI_COLLABORATION.md`, the current `docs/stages/STAGE_N_BRIEF.md`, relevant accepted ADRs, and `docs/process/QUALITY_POLICY.md` before scoped work.
- GitHub Actions on a Windows runner is automated software evidence. Inspect the exact run and logs; a green badge alone is insufficient.
- Do not claim visual Windows GUI validation or real MIDI/SysEx/hardware validation from offscreen, fake-transport, or hosted-runner tests. These remain pending after 09.09.2026 until the Product Owner performs them.
- Local absolute paths may be historical documentation, but they must never be a cloud prerequisite.
- Preserve hardware-test exclusions and all Stop/Ask, data-integrity, routing, and lifetime rules from the canonical documents.
