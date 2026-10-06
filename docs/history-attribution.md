# Git history and learning-fix merge

At the user's explicit request, all project commits on main, learning and reference-solutions (including tagged checkpoints) were reauthored as **MarvelousJade <fanshaoyu9@outlook.com>**. Existing committer identities were preserved. The intentionally introduced defects and verified reference restorations retain their original learning purpose.

The verified reference-solutions history was merged into main with a non-fast-forward merge. The intentional faulty checkpoints are now ancestors of main, followed by verified restorations; the current main driver remains identical to verified-baseline. The bugs were introduced for learning, not discovered on main or in production. Learning tags still select their faulty snapshots.

An external-to-history backup bundle is retained at `.git/before-attribution.bundle` for local recovery. The backup was created before initial publication. Historical snapshots may mention the original short IDs; this table maps them to the rewritten IDs. Commit trees were unchanged by the attribution rewrite.

| Original ID | Rewritten ID | Milestone |
|---|---|---|
| b6900c0 | a2d125b | Discovery |
| 9ac7422 | 18f93ed | Liveness |
| 421a6f7 | 1111a07 | Factorial |
| 19b6e4e | f0a84fd | Recovery |
| cae247e | 7c33b20 | Verified baseline |
| 0fc5d0b | e8e6bd7 | Intentional faulty A |
| 1f459b5 | 35310e1 | Verified reference fix A |
| 640d47d | fc39836 | Intentional faulty B |
| 554eacc | 5c84e34 | Verified reference fix B |
| 3c055a0 | e8213df | Final original delivery |
| 3b2d477 | 2a8bb27 | Merge reference fixes into main |

After merging and reauthoring, a fresh module/userspace rebuild and full TCG guest run passed: both workflows, boundaries, repeats, concurrency, timeout/late-notification recovery (1008 ms expiry), signal recovery, active-request unbind/removal and unload/reload. Raw output: `docs/evidence/07-merged-main.log`. The rewritten trees and authors for all 11 existing commits were checked; the driver/include/tools/tests still match verified-baseline. Backup bundle verification passed.

GitHub associates commits with an account only if the author email is associated with that account. The project-local Git author configuration now uses the requested name/email. Backup original refs are removed from the active ref namespace after preserving the bundle; unreachable local objects/reflogs may still retain old metadata.
