#!/usr/bin/env python3
"""Exercise IX selection in bounded runs using disposable synthetic code."""

from pathlib import Path
import subprocess
import sys
import tempfile


def instruction(opcode, register, displacement, relative=False):
    return opcode << 18 | register << 16 | int(relative) << 15 | displacement & 4095


def main():
    simulator = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="sim4000-ix-runtime-") as tmp:
        directory = Path(tmp)
        monitor = directory / "synthetic.monitor"
        # Minimum installed core is 16384 halfwords: load at 8192, entry 8198.
        # Two calls: AL W1,1; IX W1 with a PC-relative descriptor eight bytes on.
        words = [0, 0, 0, instruction(11, 1, 1),
                 instruction(61, 1, 8, relative=True), 0x004001,
                 instruction(11, 0, 0), 0, 100, 40, 0]
        contents = b"".join(word.to_bytes(3, "big") for word in words)
        monitor.write_bytes(contents)
        config = directory / "RC4000.cfg"
        traces = {}
        for index, (setting, options, enabled) in enumerate((
            (None, (), True), ("YES", (), True), ("NO", (), False),
            ("YES", ("--no-ix",), False), ("NO", ("--ix",), True),
            ("YES", ("--no-ix", "--ix"), True),
            ("NO", ("--ix", "--no-ix"), False),
        )):
            config.write_text("MONITOR FILE=synthetic.monitor\nCORESIZE=0,16384\n" +
                              (f"CPU IX={setting}\n" if setting else ""), encoding="ascii")
            trace = directory / f"run-{index}.trace"
            result = subprocess.run(
                [str(simulator), "--run", "-c", str(config), "--steps", "2",
                 "--trace", str(trace), *options],
                stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10,
                cwd=directory)
            assert result.returncode == 0, result.stdout + result.stderr
            assert "reason=step-limit calls=2 instructions=2" in result.stdout
            assert f"opcode=61({'ix' if enabled else 'unassigned'})" in result.stdout
            lines = trace.read_text(encoding="ascii").splitlines()
            columns = next(line for line in lines if line.startswith("# S\t"))[2:].split("\t")
            steps = [dict(zip(columns, line.split("\t")))
                     for line in lines if line.startswith("S\t")]
            assert len(steps) == 2
            first, last = steps
            assert first["w1"] == "000001" and first["interrupts"] == "0"
            assert last["opcode"] == "61" and last["delta"] == "1"
            assert last["address"] == "002008" and last["effective"] == "002010"
            assert last["w1"] == ("000066" if enabled else "000001")
            assert last["ic"] == ("00200c" if enabled else "000000")
            assert last["interrupt-delta"] == ("0" if enabled else "1")
            writes = [line.split("\t") for line in lines if line.startswith("W\t")]
            if enabled:
                assert not writes, writes
            else:
                assert any(row[2] == "00000a" and row[4] == "00200a" for row in writes), writes
            # Selection must not introduce nondeterminism within either profile.
            data = trace.read_bytes()
            if enabled in traces:
                assert traces[enabled] == data
            traces[enabled] = data
            assert monitor.read_bytes() == contents
    print("Bounded IX runs, override precedence, exception state and deterministic traces passed")


if __name__ == "__main__":
    main()
