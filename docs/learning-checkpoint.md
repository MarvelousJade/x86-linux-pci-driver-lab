# Intentionally faulty checkpoint B

Agent-authored learning defect, introduced after the verified reference restoration of A (`1f459b5`). The driver before this injection was identical to verified-baseline. A is no longer faulty in this checkpoint. This is not a production incident or the learner's investigation.

Ordinary liveness/factorial requests should pass. A timeout followed by another request violates the refusal/recovery contract. Reproduction: docs/exercises.md, Exercise B. Acceptance for preserving this checkpoint: successful compilation and guest selftest, then a real focused recovery regression failure (LAB FAIL, not a host launch failure). Keep the original serial logs.

Solutions are separate under solutions/. Do not inspect them until you choose to finish investigating.
