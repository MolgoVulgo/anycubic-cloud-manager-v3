# Appendix — MQTT print capture analysis

Status: `SNAPSHOT`.

The capture-derived workflow analysis showed repeated print sequences with the same broad structure:

```text
HTTP print command
-> MQTT print/start
-> download/progress
-> work report / busy state
-> automatic operation checks
-> preheating
-> printing
-> finished or unresolved terminal state
```

Observed job rows included multiple `.pwsz` files, task ids, progress steps, a single preheating transition per job, many printing updates and sometimes explicit finished messages.

Important findings:

- `action`, `state` and `type` are core parsing keys.
- `taskid` is required to segment jobs.
- Two consecutive `start` events can belong to different task ids and must not be merged blindly.
- Printer busy/free state between starts is useful but not sufficient by itself.
- MQTT observation starts after the HTTP command and becomes the main source for job state.

Integration acceptance criteria:

- detect active task id;
- map preheating and printing transitions;
- update progress monotonically unless a new task starts;
- recognize explicit terminal states;
- preserve unknown messages for diagnostics;
- do not crash on duplicate or out-of-order messages.
