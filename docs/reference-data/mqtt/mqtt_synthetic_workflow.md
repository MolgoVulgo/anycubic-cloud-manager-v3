# Synthetic MQTT print workflow

Status: `REFERENCE`.

This fixture demonstrates the parser and state-flow vocabulary. It is not a broker capture and must not be used as proof that every printer model emits the same sequence.

```text
HTTP print command accepted
-> MQTT print/update/downloading
-> MQTT print/start/preheating
-> MQTT print/start/printing
-> MQTT print/start/finished
-> MQTT status/workReport/free
```

Key rules retained by the example:

- HTTP acceptance is not final print confirmation;
- `taskid` binds download and print reports;
- `topic + action + type + state + taskid` forms the interpretation context;
- MQTT transitions update the realtime store;
- unknown messages remain observable without breaking the store.

Files:

- [`mqtt_synthetic_sample.jsonl`](mqtt_synthetic_sample.jsonl): short ordered sequence;
- [`mqtt_synthetic_summary.json`](mqtt_synthetic_summary.json): aggregate description.
