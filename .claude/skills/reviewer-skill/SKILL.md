---
name: reviewer-skill-py
description: A skill for reviewing code and providing feedback
agent: code-reviewer
disable-model-invocation: true
allowed-tools: Read, Grep, Bash
---

# Reviewer Skill

1. review code files and provide feedback on potential issues, improvements, and best practices.
2. Use the Read tool to read code files and understand their content.
3. Use the Grep tool to search for specific patterns or keywords in the code.
4. Use the Bash tool to run any necessary commands for code analysis or testing. (use uv for python code)
5. Provide clear and actionable feedback to the user based on the code review findings.
6. Ensure that the feedback is constructive and helps the user improve their code quality.
7. Focus on identifying potential bugs, security vulnerabilities, code smells, and areas for optimization.
8. Prioritize feedback based on the severity of issues and the impact on code functionality and maintainability.
9. Avoid providing feedback on stylistic preferences unless they significantly impact code readability or consistency.
10. Always aim to enhance the user's understanding of best coding practices and encourage continuous learning and improvement.  
11. Make GitHub milestones and issues for each feedback.
12. Githube milestones should be created for each feedback, and issues should be created for each specific point of feedback within those milestones.
13. Each GitHub issue should be linked to the corresponding milestone to ensure proper organization and tracking of feedback. Each issue title should start with [<severity>] to indicate the severity level of the issue (e.g., [High], [Medium], [Low]).
14. github issue should include a clear description of the issue, steps to reproduce (if applicable), and any relevant code snippets or references to the codebase.
15. Ensure that the created GitHub issues are well-organized and categorized appropriately for easy tracking and resolution by the development team.
16. after fixing the code, make a pull request and link it to the corresponding GitHub issue to facilitate the review and merging process. And the pull request should have a verification section for user to confirm that the issue has been resolved and the code changes are satisfactory.
17. Do not run unit tests or integration tests. Leave them to the user to run and verify the code changes.