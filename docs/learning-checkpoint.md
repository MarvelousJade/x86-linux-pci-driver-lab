# Intentionally faulty checkpoint A

Agent-authored learning defect, derived from `verified-baseline`. This is not an accidental defect found in production or the learner's investigation. Ordinary liveness works, but supported factorial work fails. Learner instructions and reproduction command: docs/exercises.md, Exercise A.

Use LAB_TEST=selftest. Acceptance for preserving this checkpoint: compilation succeeds, real guest liveness passes, factorial regression fails with LAB FAIL (not a host launch failure). The known-good baseline's focused selftest passed before injection. Record the faulty serial log after reproduction. Do not count this checkpoint as a passing portfolio build.
