# Appendix — Anycubic resin file extensions

Status: `SNAPSHOT`.

This appendix is a compact reference for Anycubic/Photon-related file families that can appear in cloud files or viewer research.

| Extension | Family | Notes |
| --- | --- | --- |
| `.pwmb` | Photon Workshop Binary | Main format targeted by the PWMB table-first parser. |
| `.pws` | Photon Workshop | Layer/image format family used by several Anycubic workflows. |
| `.phz` | Photon zipped/container family | Requires archive/container handling. |
| `.photons` | Photon S-style family | Legacy Photon family requiring its own driver. |
| `.pwsz` | Photon Workshop zipped/scene-line family | Cloud print captures often show `.pwsz` jobs. |

The file extension alone is not enough to guarantee compatibility with a given printer. Compatibility must come from cloud metadata, printer model, machine profile and observed API response.
