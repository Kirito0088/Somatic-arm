## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues at https://github.com/Kirito0088/Somatic-arm (via the `gh` CLI). See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.

### Arduino toolchain

Compile, upload and monitor the firmware under `firmware/` with the IDE-bundled `arduino-cli` via the shell (no MCP server). Upload/monitor need the board plugged in, so ask first. Log EMG sessions with `tools/log_emg.py`. See `docs/agents/arduino-cli.md`.
