# Exercise A reference solution — spoilers

Do not read before investigating. This is an agent-introduced defect and an agent reference fix, not learner reasoning or a historical incident.

Faulty checkpoint: learning-exercise-a (`0fc5d0b`), based on verified-baseline (`cae247e`). Real guest liveness passed, but factorial ioctl returned ETIMEDOUT and selected regression failed. See docs/evidence/exercise-a-fault.log.

Root cause: status IRQ-enable mask changed from 0x80 to 0x40. EDU documentation defines bit 7 (0x80) to request factorial completion interrupts. The computation can finish without its required interrupt, so a polling-only result check could hide the defect; our ABI correctly requires acknowledged interrupt completion.

Minimal fix: restore the documented mask. `exercise-a.patch` contains only the driver restoration. Apply on a learner branch made from the faulty tag, not on main:
```
git apply solutions/exercise-a.patch
```
If the patch is not in that checkout, use a separately saved copy or `git show main:solutions/exercise-a.patch` after deciding to view the answer.

Regression: `source scripts/lab-env.sh; bash scripts/build-guest.sh; LAB_TEST=selftest bash scripts/run-guest.sh`. Then run the full suite. Reference restoration PASS: focused selftest (130 repeated and 100 concurrent requests) and full lifecycle/recovery suite. Logs: docs/evidence/exercise-a-fixed.log and exercise-a-fixed-full.log. No kernel BUG/Oops/WARNING/panic. This verifies the agent's reference fix; it does not claim the learner has investigated or fixed the defect.
