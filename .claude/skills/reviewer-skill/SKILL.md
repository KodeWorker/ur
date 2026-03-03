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
- github_workflow:
    batch_mode: Create all feedback items in a single batch after the audit.
    structure: Organization via Milestones (categories) and Issues (specific points).
    issue_naming: Prefix titles with severity, e.g., `[High]`, `[Medium]`, or `[Low]`.
    issue_content: Include clear descriptions, code snippets, and reproduction steps.
- resolution:
    pull_request: Submit a PR linked to the relevant Issue after fixing.
    verification: Include a mandatory "Verification Section" for user sign-off.