# Remote print workflow and resin interpretation

Status: `PARTIAL` for full printer-model coverage, `IMPLEMENTED` for the documented interpretation rules used by the current workflow.

## End-to-end print model

The remote print workflow has two phases:

```text
Cloud HTTP phase
  -> select file
  -> verify printer/file compatibility
  -> submit print order
  -> receive cloud-side acknowledgement

MQTT observation phase
  -> download to printer
  -> printer checks
  -> preheating / preparation
  -> printing
  -> finish / failure / stop
```

The application must not stop at the HTTP success response. A successful order submission only means the cloud accepted the command. The real print outcome is observed through MQTT.

## Workflow state sequence

The capture-derived print workflow includes these relevant transitions:

1. cloud command sent;
2. print/start order accepted;
3. printer availability changes;
4. download/progress reports;
5. automatic operation checks;
6. preheating starts;
7. printing starts;
8. layer/progress monitoring;
9. finished, stopped, failed or blocked.

`print/report` carries most job transitions. `status/report` provides machine availability and must be correlated with the job state.

## Parent of `start`

In MQTT payload interpretation, `start` is not meaningful alone. The effective parent context is the MQTT message family plus `type/action/state` combination.

Examples:

- `print / start / preheating` means the print workflow entered preparation/preheating.
- `resin / feedResin / start` means a resin feed operation started.
- `autoOperation` messages describe automatic checks or hardware-side operations, not necessarily print stage transitions.

The parser must therefore keep `topic`, `action`, `state`, `type`, `taskid` and print phase together.

## Resin vocabulary

| Term | Meaning |
| --- | --- |
| Vat / VAT | Resin vat on the printer. The relevant target is the vat max mark, not an arbitrary computed resin quantity. |
| Bottle / reservoir | External source used by the auto-load system. |
| Auto-load | Hardware operation that feeds resin into the vat. |
| `feedResin` | MQTT action related to resin feeding. Its meaning depends on phase. |
| `failDetection` | Failure detection path that can later turn a resin condition into a blocking print failure. |

## Two resin workflows

### A. Before preheating

This is the preparation/autoload workflow. If an autoload operation is observed and the printer later enters preheating, the vat fill step is considered successful. Otherwise, the workflow would usually expose `feedResin` as a blocking print-preparation error.

Interpretation rule:

```text
autoload observed
AND later print/preheating observed
=> pre-print resin preparation succeeded
```

Failure before preheating is blocking because the print has not reached the phase where it can proceed.

### B. During printing

Runtime refill attempts do not prove that the printer computed a precise required resin volume. On M7 Pro-class and M3 Pro-class behavior, the system fills the vat up to the max mark.

During an active print, a `feedResin` event or `code=1501` is interpreted first as a bottle/autoload source issue. It becomes a blocking material failure only when later state confirms that the print failed or failure detection escalated the condition.

Interpretation rule:

```text
printing active
AND feedResin/code=1501 observed
AND no later print failure
=> resin source/autoload warning, not immediate print failure
```

## Minimal resin state model

Recommended internal states:

```text
unknown
preprint_fill_running
preprint_fill_ok
preprint_fill_failed_blocking
runtime_refill_requested
runtime_refill_warning
runtime_material_failure
```

## Correlation windows

The state machine must correlate resin events with the active print phase and `taskid`. A resin event near the boundary between two print jobs must not be attached blindly to the wrong job.

Minimum correlation fields:

- current task id;
- previous task id;
- print phase;
- timestamp;
- last `print/report` transition;
- last `status/report` availability;
- last resin event.

## Reference case: 06/05/2026

The documented case contains a vat fill before preheating and an empty-bottle style error during the print. The correct interpretation is to distinguish the two workflows:

- the preheating transition proves the first fill path succeeded;
- the later runtime `feedResin`/bottle-empty signal is not equivalent to an empty vat;
- the print should be considered failed by resin only if the printer later emits a failure state or a failure-detection path confirms it.

## Do not do

- Do not treat every `feedResin` as a print-blocking resin fault.
- Do not assume refill attempts mean additional resin volume was calculated.
- Do not ignore print phase.
- Do not infer vat empty from bottle empty without corroborating printer failure state.
- Do not collapse pre-print autoload and runtime refill into the same state.

## Decision

The UI should display resin state as contextual information. It should escalate to blocking error only when the phase and subsequent printer state justify it.
