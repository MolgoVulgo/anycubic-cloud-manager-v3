# Appendix — Cloud client screens

Status: `IMPLEMENTED` for active screens, `PARTIAL` for draft views.

## Main window

The main window is the control room. It owns global navigation, runtime/session status and access to Files, Printers and Logs. Header actions must not bypass the main product workflow or open misleading draft tools in production.

## Files tab

Purpose:

- list cloud files;
- refresh cloud data;
- show file metadata;
- download/delete selected file;
- provide entry toward print workflow.

Expected UI sections:

- toolbar with refresh/action buttons;
- file list/table/cards;
- inline status for cache/cloud activity;
- file details dialog;
- deterministic error display.

## File details dialog

Shows readable metadata grouped as:

- general file identity;
- slicing metadata;
- cloud metadata;
- download/thumbnail references with redaction where necessary.

## Printers tab

Purpose:

- show printer cards/tabs;
- expose active printer details;
- show recent jobs;
- show MQTT overlay state;
- provide guarded print action.

Mandatory detail content:

- printer name/id/key when safe;
- machine type/model;
- cloud status;
- MQTT connection and subscription state;
- active task id;
- print stage and progress;
- resin/autoload status;
- compatibility and reason messages.

## Logs tab

Debug-only runtime log viewer. It must be absent or replaced with an explanatory disabled panel in production builds.

## Session Settings

HAR import dialog. It must display security reminders, target session path, import result and token count without exposing raw token values.

## Draft views

Upload, direct print payload and viewer dialogs must remain marked as draft or hidden from normal production paths until their backend workflows are complete.
