h# EXE Return Codes

This document catalogs miscellaneous EXE return (exit) codes emitted by the Transparency suite of executables. Use this page as the canonical reference URL when surfacing exit codes from logs, CI pipelines, or installer telemetry.

## Conventions

- `0` indicates a clean, successful run.
- Non-zero codes indicate an error or a non-default termination state.
- Codes are grouped by category. Reserve unused ranges before adding new codes.

## Reserved Ranges

| Range | Category |
|------------|------------------------------------------|
| 0          | Success |
| 1–19      | Generic / CLI argument errors |
| 20–49     | Network discovery / scanner errors |
| 50–79     | Device fingerprinting errors |
| 80–109    | Threat detection / remediation errors |
| 110–139   | Installer / updater errors |
| 140–199   | Platform-specific (Win32 / Linux / Android) |
| 200–254   | Reserved for future use |
| 255        | Catastrophic / unhandled exception |

## Miscellaneous EXE Return Codes

| Code | Symbolic Name | Description | Emitting Component |
|------|----------------|-------------|--------------------|
| 0    | `EXIT_SUCCESS` | Operation completed successfully. | All |
| 1    | `EXIT_FAILURE` | Generic, unspecified failure. | All |
| 2    | `EXIT_BAD_ARGS` | Invalid or missing command-line arguments. | CLI |
| 3    | `EXIT_CONFIG_ERROR` | Configuration file missing or malformed. | All |
| 4    | `EXIT_PERMISSION_DENIED` | Insufficient privileges (run as admin/root). | All |
| 110  | `EXIT_DISK_FULL` | Insufficient disk space to complete the operation. Maps to Win32 `ERROR_DISK_FULL` (112) and POSIX `ENOSPC` (28). | Installer, Scanner cache, Report exporter |
| _TBD_ | _TBD_ | _Add additional miscellaneous codes here._ | _TBD_ |

> Fill in the rows above as new exit codes are introduced. Keep the list alphabetized by code.

## How to Reference This Page

Use the canonical URL:

```
https://github.com/MantheDan27/transparency/blob/main/docs/EXE_RETURN_CODES.md
```

For specific rows, link to the anchor of the table heading, e.g.:

```
https://github.com/MantheDan27/transparency/blob/main/docs/EXE_RETURN_CODES.md#miscellaneous-exe-return-codes
```

## Change Log

- Initial scaffold of the EXE return-code reference document.
