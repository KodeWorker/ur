---
name: reviewer-skill
description: Efficient code review agent for bug detection, security, and PR automation.
agent: code-reviewer
disable-model-invocation: true
allowed-tools: [Read, Grep, Bash]
---

# Reviewer Skill

- objective: Scan for bugs, security vulnerabilities, and optimizations using Read/Grep.
- environment: Use Bash with `uv` for Python analysis. Do not execute unit/integration tests.
- prioritization: Focus on high-impact logic and maintainability over stylistic preferences.
- phase_scope:
    when_specified: If the user states a phase (e.g. "Phase 2"), resolve the scope before auditing.
    resolution_order:
      1. Check `docs/plans/` for a plan file matching the phase (e.g. `phase2.md`).
      2. Fall back to the `## Roadmap` section in `README.md` to identify which features and files belong to the phase.
    enforcement: Only audit files and components that belong to the stated phase. Findings in out-of-scope files must be silently dropped — do not create issues for them.
    milestone_annotation: Append the phase to the milestone title, e.g. "Review Run 2 (Phase 2)".
- github_workflow:
    structure: Organization via Milestones (review runs) and Issues (specific findings).
    milestone:
      naming: "Review Run <N>" where N increments from the highest existing run milestone (e.g. "Review Run 1", "Review Run 2").
      detection: Query existing milestones with `gh api repos/{owner}/{repo}/milestones` to find the current highest N before creating.
      due_date: Set to 2 weeks from the current date (ISO 8601, e.g. `--field due_on=2026-03-17T00:00:00Z`).
      scope: One milestone per review invocation; all issues from this run are assigned to it.
    issue_naming: Prefix titles with severity, e.g., `[High]`, `[Medium]`, or `[Low]`.
    issue_content: Include clear descriptions, code snippets, and reproduction steps.
    labels:
      setup: Before creating issues, ensure the following labels exist (create with `gh label create` if missing).
      severity: "[High] (color: d73a4a), [Medium] (color: e4a11b), [Low] (color: 0075ca)"
      category: "security (color: b60205), correctness (color: 5319e7), robustness (color: 006b75), performance (color: 0e8a16)"
      tagging: Each issue must be tagged with its severity label AND its category label.
    user_audit:
      step_1: After completing the audit, present all findings to the user as a markdown table (title, severity, category, one-line description). Do NOT create anything yet.
      step_2: Wait for explicit user approval. The user may drop, edit, or re-categorise items before approving.
      step_3: Only after approval, generate a single self-contained bash script that creates the milestone and all approved issues in one run. Print the script and ask the user to confirm before executing.
      step_4: Execute the script with a single Bash tool call.
- resolution:
    pull_request: Submit a PR linked to the relevant Issue after fixing.
    verification: Include a mandatory "Verification Section" for user sign-off.
