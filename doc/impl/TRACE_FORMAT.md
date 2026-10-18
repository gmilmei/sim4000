# SIM4000 Architectural Trace Format

The bounded runner can emit a deterministic, machine-readable trace:

```sh
./build/sim4000 --run --steps 1000 --trace startup.trace
```

The destination is created exclusively. An existing file is never replaced.
The trace is UTF-8-compatible ASCII with LF line endings and tab-separated
fields. Every number is locale independent. Addresses and RC4000 words are
six-digit lowercase hexadecimal; counters and individual fields are decimal.

The first line identifies the format and its version:

```text
SIM4000-TRACE	1
```

Lines beginning with `#` describe the columns and may be ignored. Consumers
must reject unsupported versions, accept additional comment lines, and use the
fixed columns for that version rather than parsing the descriptive comments.

## Version 1 records

`W` records describe core-store write operations:

```text
W  call  address  previous  new
```

The fields are separated by tabs in an actual file. `address` is the even byte
address of the 24-bit word. Writes to the four W-register aliases are reflected
in the following state record and do not produce `W` records. Multiple writes
associated with one call appear in execution order before its `S` record. This
also includes deterministic device service immediately preceding that call.

`S` records contain the state after one interpreter call:

```text
S  call  result  instructions  delta  address  raw  opcode  mode  index
   result-register  displacement  effective  ic  w0  w1  w2  w3  ir  im
   ex21  ex22  ex23  interrupts-enabled  monitor  reset  stopped
   io-operations  io-delta  last-device  last-command  last-modifier
   interrupts  interrupt-delta  ticks
```

`call` starts at one. `instructions` is the cumulative architectural
instruction count and `delta` is the number consumed by this interpreter call;
the latter can exceed one when address-modification prefixes are processed.
`address` through `effective` describe the last instruction. `io-delta` and
`interrupt-delta` identify calls containing an I/O operation or interrupt
entry. Register state after an I/O instruction contains transferred data or
returned status where the architecture defines it.

The numeric `result` values are stable for version 1:

- `0`: completed normally
- `1`: stopped
- `2`: address exception
- `3`: illegal instruction
- `4`: unimplemented instruction

A trace interrupted by a catchable host signal remains a valid prefix and is
closed during normal signal cleanup. A host I/O error makes the runner fail.
For reproducible comparisons, use identical input artifacts and options and do
not provide asynchronous console or Telnet input.

Use `dev/python/trace_compare.py EXPECTED ACTUAL` to compare two version-1 traces.
It ignores descriptive comments but validates record shapes and reports the
first differing record and named field.
