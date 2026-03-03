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
- github_workflow:
    structure: Organization via Milestones (review runs) and Issues (specific findings).
    milestone:
      naming: "Phase <N> - Code Review Run <M>" where N is the phase number (0 if no phase stated) and M increments from the highest existing run for that phase (e.g. "Phase 2 - Code Review Run 1", "Phase 2 - Code Review Run 2").
      detection: Query existing milestones with `gh api repos/{owner}/{repo}/milestones` to find the current highest M for the given phase before creating.
      due_date: Set to 2 weeks from the current date (ISO 8601, e.g. `--field due_on=2026-03-17T00:00:00Z`).
      scope: One milestone per review invocation; all issues from this run are assigned to it.
    issue_naming: Prefix titles with severity, e.g., `[High]`, `[Medium]`, or `[Low]`.
    issue_content: Include clear descriptions, code snippets, and reproduction steps.
    labels:
      setup: Before creating issues, fetch existing labels with `gh label list --repo {owner}/{repo} --json name,color` and use them as the source of truth.
      severity: Ensure "[High]" (color: d73a4a), "[Medium]" (color: e4a11b), "[Low]" (color: 0075ca) exist — create with `gh label create` only if missing.
      category: Read category labels from the repo. Do not assume or predefine any category labels. Match each finding to the closest existing label by name. If no suitable label exists, create one using a name and color that fits the finding's theme.
      tagging: Each issue must be tagged with its severity label AND its category label.
    user_audit:
      step_1: After completing the audit, present all findings to the user as a markdown table (title, severity, category, one-line description). Do NOT create anything yet.
      step_2: Wait for explicit user approval. The user may drop, edit, or re-categorise items before approving.
      step_3: Only after approval, generate a single self-contained bash script that creates the milestone and all approved issues in one run. Print the script and ask the user to confirm before executing.
      step_4: Execute the script with a single Bash tool call.
      step_5: After the script succeeds, create and checkout a new branch named "fix/phase-<N>-code-review-<M>" (matching the milestone numbers) with `git checkout -b fix/phase-N-code-review-M`.
- resolution:
    user_audit:
      step_0: Append a code review report section to `docs/devlog/<YYYY>-<MM>-<DD>.md` (today's date). Create the file if it does not exist. The section should include the milestone name, a table of all findings (issue number, title, severity, category), and a link to each GitHub issue.
      step_1: Before touching any code, present a fix plan to the user — list each issue with the proposed change (file, location, and approach). Do NOT modify any files yet.
      step_2: Wait for explicit user approval. The user may drop, defer, or adjust individual fixes before approving.
      step_3: Only after approval, apply the approved fixes.
    pull_request: After fixes are applied, submit a PR linked to the relevant issues.
    issue_closing:
      format: Each issue must be closed with its own keyword — "Closes #1, Closes #2" (one keyword per issue, comma-separated).
      invalid: Do NOT use "Closes #1 #2" (single keyword for multiple issues — GitHub does not parse this correctly).
      placement: Include the closing keywords in the PR body, one per line or comma-separated in a single line.
    verification: Include a mandatory "Verification Section" for user sign-off.
