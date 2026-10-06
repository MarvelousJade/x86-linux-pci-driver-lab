# Debugging exercises (learner instructions — no solutions)

These are **intentionally introduced learning defects**, not production incidents. Main must stay passing. Two independent faulty checkpoints are preserved on the `learning` branch with neutral tags; use the checkpoint tag, not an arbitrary later commit.

| Checkpoint | Faulty commit | Regression | Observed result |
|---|---|---|---|
| learning-exercise-a | e8e6bd7 | selftest | liveness passes, factorial fails |
| learning-exercise-b | fc39836 | recovery | ordinary selftest passes, recovery fails |

Both derive from the verified-baseline (`7c33b20`) driver; B follows the separately verified restoration of A. The reference restorations passed their focused regressions and full guest suite. Those reference patches/root causes are in the separate solutions directory, not these instructions.

Run exercises only in the disposable QEMU guest. Do not merge faulty code into main. Preserve your own investigation notes before looking at a solution. Solutions and patches are stored separately under `solutions/`; this page contains no progressive hints. Ask for a hint when you want one.

## Exercise A

Symptom: the liveness path works, but ordinary supported factorial work fails. Use the `learning-exercise-a` checkpoint and focused selftest regression.

```
git switch --detach learning-exercise-a
source scripts/lab-env.sh
bash scripts/build-guest.sh
LAB_TEST=selftest bash scripts/run-guest.sh
```

Expected faulty outcome: the regression fails (no LAB PASS). Investigate the request from userspace through the hardware/IRQ path. Keep evidence, then implement your proposed fix on a new learner branch. Rerun the same test and the complete suite.

## Exercise B

Symptom: timeout injection occurs, but the next request does not satisfy the refusal/recovery contract. Use `learning-exercise-b` and focused recovery regression. This checkpoint is independent of Exercise A; the earlier defect was restored before introducing this one.

```
git switch --detach learning-exercise-b
source scripts/lab-env.sh
bash scripts/build-guest.sh
LAB_TEST=recovery bash scripts/run-guest.sh
```

Expected faulty outcome: the regression fails. Trace request ownership at timeout and retry. The notification hold is a deliberate test hook, not itself the defect. Record which input belongs to each request and what each return value means.

## Notes to retain

- Exact tag/commit and environment, command and observed symptoms.
- Problem -> hypothesis -> evidence -> decision -> fix -> verification.
- Hypotheses you rejected and why; your minimal patch; regression/full-suite output.

After saving any learner changes, `git switch main` returns to the passing project. Run `source scripts/lab-env.sh; bash scripts/verify-main.sh` to rebuild (never trust a .ko left over from an exercise).
