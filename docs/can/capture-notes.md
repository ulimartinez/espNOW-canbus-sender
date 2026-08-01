# Capture notes

Log real-vehicle CAN capture sessions here as they happen -- one section per session, so decoder assumptions can be checked against actual bus traffic over time. Use `scripts/capture_can.sh` (or `can start` / `can filter ...` over Serial) to collect raw frames first.

Suggested per-session template:

```markdown
## YYYY-MM-DD -- <car state: idle / driving / etc.>

- Firmware version / commit:
- Duration:
- IDs observed (from `can summary`):
- Anything that didn't match docs/can/signal-map.md:
- Follow-ups:
```

No sessions logged yet -- this firmware has not been run against real vehicle CAN traffic.
