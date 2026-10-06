# Exercise B reference solution — spoilers

Do not read before investigating. This is an agent-introduced defect/reference restoration, not learner reasoning or a production incident.

Faulty checkpoint: learning-exercise-b (`fc39836`), introduced after A was restored (`35310e1`). Its ordinary selftest passes (exercise-b-baseline.log); its recovery regression fails: the next request times out instead of returning EBUSY while the old notification is held (exercise-b-fault.log).

Root cause: the request admission check was reduced to the hardware BUSY bit. QEMU clears BUSY before raising its completion IRQ, and our test hook may retain an old software notifier even after the IRQ is acknowledged. Idle is not permission to overwrite pending/irq_seen/notified or reinitialize the same completion. This defect drops the software ownership condition.

Minimal fix: also reject a pending request until both irq_seen and notified are true. `exercise-b.patch` restores that guard. irq_lock serializes this guard and completion preparation against the ISR and notifier. Late work cannot be reassigned to a newer request while unresolved. No hardware reset is involved.

Observed failure is a contract violation/retry timeout; no corrupted result or interrupt storm was observed or claimed. A race-only completion-initialization exercise was not used because fast EDU computation would not give a reliably reproducible lost wakeup without additional artificial ordering hooks.

Regression: `source scripts/lab-env.sh; bash scripts/build-guest.sh; LAB_TEST=recovery bash scripts/run-guest.sh`, then the full suite. Apply the patch only to a new learner branch from the faulty tag, using a separately saved patch if needed. Reference restoration PASS: focused recovery (1028 ms expiry) and complete baseline/concurrency/lifecycle suite (1007 ms recovery expiry). Logs: docs/evidence/exercise-b-fixed.log and exercise-b-fixed-full.log. No kernel BUG/Oops/WARNING/panic. This verifies the agent reference fix, not the learner's eventual investigation.
