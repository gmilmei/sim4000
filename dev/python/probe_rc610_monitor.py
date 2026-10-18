#!/usr/bin/env python3
"""Inspect compiled monitor peripherals on an exclusive volatile working copy."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
OPTION_SHA256 = "47294b7b4455a52bbc69022c815842b61b0fcb42fde77877fe9b190cfc3f03a5"
sys.path.insert(0, str(ROOT / "tests/python"))
from test_operator_cli import Session


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def peripherals(core):
    """Follow live b4..b5 peripheral slots, not arbitrary embedded strings.

    montxt2 places the monitor table at RC address 8: b0=16, b3=72,
    b4=74, b5=76, b6=78, b7=80. montxt1 gives peripheral field offsets.
    """
    if len(core) != 524288 * 3:
        raise ValueError("expected a complete 1048576-halfword snapshot")

    def word(address):
        if address < 0 or address % 2 or address + 2 > 1048576:
            raise ValueError("invalid core word address")
        index = address // 2 * 3
        return int.from_bytes(core[index:index + 3], "big")

    first, top = word(74), word(76)
    if first % 2 or top % 2 or not 8 <= first < top < 1048576 or top - first > 4096:
        raise ValueError("invalid live peripheral name-table interval")
    records = []
    for slot in range(first, top, 2):
        address = word(slot)
        if address == 0:
            continue
        if address % 2 or not 8 <= address < 1048500:
            raise ValueError("invalid peripheral descriptor pointer")
        index = (address + 2) // 2 * 3
        name = core[index:index + 12].split(b"\0", 1)[0].decode("ascii")
        kind = word(address)
        if not name:
            continue
        records.append({"name": name, "kind": kind, "descriptor": address,
                        "device_number": (slot - first) // 2,
                        "slot": slot, "device_field": word(address + 10),
                        "interrupt_entry": word(address + 20),
                        "parameters": [word(address + offset) for offset in range(22, 36, 2)]})
    # Independent controls guard against silently reading the wrong table.
    if not any(row["name"] == "reader" and row["kind"] == 10 for row in records) or \
            not any(row["name"] == "punch" and row["kind"] == 12 for row in records):
        raise ValueError("live peripheral table lacks known reader/punch controls")
    return records


def prepare_sources(destination):
    """Copy the printer-enabled maintained inputs into a new private tree."""
    source = ROOT / "monsrc"
    option = source / "sim4000.mops"
    data = option.read_bytes()
    printer = b"t.18 i.10 d.7 n.printer             ; RC610 line printer\n"
    if digest(option) != OPTION_SHA256 or data.count(printer) != 1:
        raise ValueError("unrecognized or already changed monitor options")
    before = {p: digest(p) for p in source.iterdir() if p.is_file()}
    shutil.copytree(source, destination)  # Refuse any existing destination.
    assert {p: digest(p) for p in before} == before
    (destination / "printer-option.json").write_text(json.dumps({
        "transform": "copy-maintained-printer-inputs-v1", "device": 7, "interrupt": 10,
        "option_source_sha256": before[option],
        "option_file": "sim4000.mops",
        "option_candidate_sha256": digest(destination / "sim4000.mops"),
        "promoted": True,
    }, indent=2) + "\n", encoding="ascii")


def run(simulator, monitor, destination, expected_printer=False):
    source = ROOT / "src/disks/UNIT0"
    guarded = {source: digest(source), monitor: digest(monitor)}
    destination.mkdir()  # Refuse old evidence and personal runtime state.
    shutil.copyfile(source, destination / "UNIT0")
    shutil.copyfile(monitor, destination / "MONITOR")
    baseline = {name: digest(destination / name) for name in ("UNIT0", "MONITOR")}
    (destination / "RC4000.cfg").write_text(
        "PROTECTION MODE=OFF\nCPU IX=YES\nMONITOR FILE=MONITOR\nCORESIZE=0,1048576\n"
        "THROTTLE VALUE=OFF\nDEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
        "DEFINE CLOCK DEVNO=3 INTNO=11\nDEFINE RC433X DEVNO=15 FILE=UNIT0\n",
        encoding="ascii")
    session = None
    with (destination / "session.log").open("x", encoding="ascii") as log:
        def record(text):
            log.write(text)
            log.flush()
            return text
        try:
            session = Session(simulator, destination, ("-a",), startup=b"initialize date:")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            os.write(session.master, b"\x1b")
            record(session.until(b"att") + session.until(b"\x07"))
            os.write(session.master, b"s\r")
            record(session.until(b"\x07"))
            os.write(session.master, b"all prinspect size 150000 run\r")
            record(session.until(b"to prinspect") + session.until(b"\x07"))
            for name in ("disc", "printer", "printer1"):
                os.write(session.master, f"montest external name.{name}\r".encode("ascii"))
                output = record(session.until(b"\x07", timeout=20))
                (destination / f"{name}.txt").write_text(output, encoding="ascii")
            os.write(session.master, b"finis\r")
            record(session.until(b"pause prinspect finis fp"))
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            record(session.command(b"save-core monitor-core.bin 0 524288\n"))
            records = peripherals((destination / "monitor-core.bin").read_bytes())
            printers = [row for row in records if row["kind"] == 14]
            assert bool(printers) == expected_printer, printers
            core = (destination / "monitor-core.bin").read_bytes()
            vectors = [int.from_bytes(core[(16 + 2 * level) // 2 * 3:
                                           (16 + 2 * level) // 2 * 3 + 3], "big")
                       for level in range(25)]
            if expected_printer:
                assert len(printers) == 1
                printer = printers[0]
                assert (printer["name"], printer["device_number"], printer["device_field"],
                        printer["parameters"][3]) == ("printer", 7, 448, 451), printer
                assert vectors[10] == printer["descriptor"] - 6, (vectors[10], printer)
                display = (destination / "printer.txt").read_text(encoding="ascii")
                assert f"core. {printer['descriptor']}" in display
                assert "kind                 :           14" in display
                assert "device number :   7" in display
            else:
                assert all(row["device_number"] != 7 for row in records)
                assert vectors[10] == vectors[8], "interrupt 10 is not the unused-handler control"
            result = {"monitor_sha256": baseline["MONITOR"], "peripherals": records,
                      "interrupt_vectors": vectors,
                      "printer_descriptors": printers, "printer_io_tested": False,
                      "source_and_base_hashes_unchanged": True}
            (destination / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="ascii")
            session.finish()
        finally:
            if session:
                session.close()
            assert {name: digest(destination / name) for name in baseline} == baseline
            assert {path: digest(path) for path in guarded} == guarded


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--simulator", type=Path, default=ROOT / "build-rc610/sim4000")
    parser.add_argument("--monitor", type=Path, default=ROOT / "src/monitor/MONITOR")
    parser.add_argument("--expect-printer", action="store_true", default=True,
                        help="require the installed printer (the maintained default)")
    parser.add_argument("--prepare-sources", action="store_true",
                        help="only prepare an exclusive private RC610 source candidate")
    args = parser.parse_args()
    if args.prepare_sources:
        prepare_sources(args.destination.resolve())
        print("Private RC610 option sources prepared; no monitor rebuilt or promoted.")
        return
    run(args.simulator.resolve(), args.monitor.resolve(), args.destination.resolve(), args.expect_printer)
    print("Compiled monitor peripheral inventory verified; no printer I/O claimed.")


if __name__ == "__main__":
    main()
