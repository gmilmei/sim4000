#!/usr/bin/env python3
"""Pure guards for isolated MIPS/TS inventory and component-listing probes."""

from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from make_unit0_copy import CopyError
from prepare_mips_test import autostart_text, configuration, instructions
from mips_primo import DEVICE_COMMANDS, verify_primo_devices, verify_primo_transfer
from test_mips_printer import (verify_completion, verify_offline,
                               verify_queued_definitions, verify_queued_completion,
                               held_definition, verify_primo_start, verify_primo_trimming)
from test_mips_batch_manual import manual_recipe as batch_recipe, verify_password_frame
from mips_terminal import (verify_interactive_child, verify_interactive_message,
                           verify_interactive_lookup, verify_interactive_logout,
                           verify_interactive_enrollment)
from probe_mips_ts import (TEXTS, BINARIES, EMPTY_CATALOG, inspect, verify_inventory,
                           verify_component_listing, verify_primo_reply,
                           verify_empty_catalog, private_sos_start, read_system_reply,
                           verify_sos_startup, verify_account_catalog, account_catalog_bytes,
                           validate_operator_commands, verify_operator_frame, catalog_outcome,
                           locate_parser_code, PARSER_OFFSET, PARSER_SIGNATURE,
                           verify_terminal_catalog, terminal_catalog_bytes, sos_reply_event,
                           verify_second_disc, verify_second_disc_file, SECOND_DISC_TEXT,
                           SECOND_DISC_SEGMENTS, verify_sos_disc1_storage, verify_batch_enrollment,
                           batch_core_state, verify_batch_job, BATCH_JOBS, verify_retained_batch_file)


class MipsProbeTests(unittest.TestCase):
    def test_batch_manual_blocks_match_checked_dialogue(self):
        self.assertEqual(len(batch_recipe()), 8)

    def test_batch_password_forms_require_correct_context_and_echo(self):
        prompt = "batch test jobfile batchjob\r\n>password \x07"
        inline = "batch test jobfile batchjob pass test\r\n08.00 sos: test enrolled\r\n"
        verify_password_frame("password", prompt, False)
        verify_password_frame("response", inline, True)
        for kind, reply, form in (("response", prompt, False), ("password", inline, True),
                                 ("password", prompt.replace("jobfile batchjob", "other"), False),
                                 ("password", prompt + ">password \x07", False),
                                 ("response", inline + ">password \x07", True)):
            with self.assertRaises(AssertionError):
                verify_password_frame(kind, reply, form)

    def test_primo_trimming_requires_unique_known_generation_options(self):
        text = "waitops := 1, waittrans := 300, prcount := 3, oprcount := 4, options := 941109,"
        verify_primo_trimming(text)
        for bad in (text.replace("waitops := 1", "waitops := 2"), text + "waitops := 2,",
                    text.replace("waittrans := 300,", ""), text.replace("941109", "861101")):
            with self.assertRaises(AssertionError):
                verify_primo_trimming(bad)

    def test_held_acknowledgement_and_notice_may_arrive_either_way(self):
        definition = "primo define 0 reply 0 transport 246\r\n"
        notice = "08.00 primo:***printer end document\r\n"
        for output in (definition + notice, notice + definition):
            self.assertEqual(held_definition(output), (definition, notice))
        for bad in (definition, notice, definition * 2 + notice, definition + notice * 2,
                    definition + notice + "***other error", definition + notice + "end 28\r\n",
                    definition.replace("246", "0") + notice):
            with self.assertRaises(AssertionError):
                held_definition(bad)

    def test_private_primo_recreation_checks_original_startup(self):
        original = ("* new primo size 16938 function 1 2 3 4 5 buf 10 area 7\n"
                    "* perm disc 18 3 base -8388607 8388605 prog bprimo\n"
                    "* run\n* unstack\n\f\n")
        verify_primo_start(original)
        for bad in (original.replace("buf 10", "buf 11"), original + "* run\n",
                    original.replace("bprimo", "other"), original.replace("* unstack", "")):
            with self.assertRaises(AssertionError):
                verify_primo_start(bad)

    def test_two_outstanding_transports_need_distinct_accepted_ids(self):
        first = "qone\r\nprimo define 0 reply 0 transport 246\r\n"
        second = "qtwo\r\nprimo define 0 reply 0 transport 328\r\n"
        self.assertEqual(verify_queued_definitions(first, second), [246, 328])
        for bad in (second.replace("328", "246"), second.replace("328", "0"),
                    second.replace("reply 0", "reply 3"), second + second,
                    second + "primo wait 0 reply 0 state 5 cause -1\r\n"):
            with self.assertRaises(AssertionError):
                verify_queued_definitions(first, bad)

    def test_queued_jobs_need_two_normal_completed_clients(self):
        reply = ("from prbprep\r\nprimo wait 0 reply 0 state 5 cause -1\r\nend 28\r\n"
                 "from prbnext\r\nprimo poll 0 reply 0 state 5 cause -1\r\nend 30\r\n")
        payloads = (b"First private text.\n", b"Second private text.\n")
        verify_queued_completion(reply, payloads)
        for bad in (reply.replace("prbnext", "prbprep"), reply.replace("poll 0", "poll 1"),
                    reply.replace("reply 0 state 5", "reply 3 state 0"),
                    reply.replace("end 30", "abnormal end 30"), reply + reply,
                    reply + "Second private text.\r\n"):
            with self.assertRaises(AssertionError):
                verify_queued_completion(bad, payloads)

    def test_physical_printer_completion_rejects_false_success(self):
        reply = ("primo define 0 reply 0 transport 246\r\n"
                 "primo wait 0 reply 0 state 5 cause -1\r\nend 26\r\n\x07")
        self.assertEqual(verify_completion(reply, b"Private output.\nSecond line.\n"), 246)
        for bad in (reply + reply, reply.replace("transport 246", "transport 0"),
                    reply.replace("state 5", "state 4"), reply.replace("cause -1", "cause 1"),
                    reply.replace("end 26", "abnormal end 26"), reply + "***error",
                    reply + "Second line.\r\n"):
            with self.assertRaises(AssertionError):
                verify_completion(bad, b"Private output.\nSecond line.\n")

    def test_printer_offline_requires_waiting_not_success(self):
        notice = "from primo\r\n08.00 primo:***printer end document\r\n"
        display = "printer ..0....0 printer pin console1 ,waiting \r\n08.00 primo:ready\r\n"
        verify_offline(notice, display)
        for bad_notice, bad_display in ((notice.replace("end document", "ready"), display),
                (notice + "primo wait 0 reply 0 state 5 cause -1", display),
                (notice + "end 26\r\n", display), (notice, display.replace("waiting", "idle")),
                (notice, display.replace(" pin ", " other "))):
            with self.assertRaises(AssertionError):
                verify_offline(bad_notice, bad_display)

    def test_primo_device_fixture_does_not_infer_nonexistent_consoles(self):
        observations = []
        unavailable = {"signup console3 14", "signup console4 14", "signoff console3",
                       "signoff console4", "signoff punch", "signoff reader"}
        unsupported = {"signup punch 12", "signup reader 10", "signup console2 8"}
        for index, command in enumerate(DEVICE_COMMANDS):
            result = "***device unknown" if command in unavailable else \
                     "***no resources" if command in unsupported else "ready"
            rows = "console1 ,idle\r\nconsole2 ,idle\r\n" if index == 5 else ""
            observations.append({"command": command,
                                 "reply": command + "\r\n" + rows + "07.59 primo:" + result + "\r\n"})
        verify_primo_devices(observations)
        for index in (3, 4, 8, 9):
            altered = [dict(item) for item in observations]
            altered[index]["reply"] = altered[index]["reply"].replace("***device unknown", "ready")
            with self.assertRaises(CopyError):
                verify_primo_devices(altered)
        altered = [dict(item) for item in observations]
        altered[5]["reply"] += "console3 ,idle\r\n"
        with self.assertRaises(CopyError):
            verify_primo_devices(altered)

    def test_primo_transport_requires_exact_output_and_completion(self):
        reply = ("prbprog\r\n\r\nfrom primo\r\nPRIMO private transport probe.\r\n"
                 "from prbprep\r\nprimo define 0 reply 0 transport 82\r\n"
                 "primo wait 0 reply 0 state 5 cause -1\r\nend 26\r\n\x07")
        self.assertEqual(verify_primo_transfer(reply), 82)
        for bad in (reply + reply, reply.replace("state 5", "state 4"),
                    reply.replace("transport 82", "transport 0"),
                    reply.replace("PRIMO private", "PRIMO wrong"), reply + "***error"):
            with self.assertRaises(CopyError):
                verify_primo_transfer(bad)

    def test_disposable_autostart_avoids_third_input_stack_level(self):
        original = b"* all startproc size 50000 i 4.startarea run\n* unstack\nstartdate c\nfinis\n"
        boot = autostart_text(original)
        self.assertEqual(boot, b"* kit 16\n* date 86 09 30 08 00 00\n"
                         b"* read starttem\n* read startprimo\n* read startsos\n* unstack\n")
        for bad in (b"", original + original, original.replace(b"startdate c", b"unknown")):
            with self.assertRaises(CopyError):
                autostart_text(bad)

    def test_disposable_runtime_is_loopback_overlay_only(self):
        config = configuration(4004)
        self.assertIn("ADDRESS=127.0.0.1 PORT=4004 ATTENTION=CTRL-A", config)
        self.assertIn("FILE=UNIT0 OVERLAY=UNIT0.overlay", config)
        self.assertIn("FILE=UNIT1 OVERLAY=UNIT1.overlay", config)
        self.assertNotIn("WRITABLE=YES", config)
        self.assertNotIn("\r", config)
        preparation = configuration(4004, True)
        self.assertNotIn("TELNET", preparation)
        self.assertEqual(preparation.count("WRITABLE=YES"), 2)
        self.assertIn("test environment", instructions(4004))
        self.assertIn("127.0.0.1 4004", instructions(4004))

    def test_interactive_run_requires_tem_header_but_go_does_not(self):
        enrolled = "08.00 sos: test enrolled\r\n"
        header = "\r\nfrom tem\r\n"
        verify_interactive_enrollment(enrolled, "go")
        verify_interactive_enrollment(header + enrolled, "run")
        for output, mode in ((enrolled, "run"), (header + enrolled, "go"),
                             (header + header + enrolled, "run"),
                             (enrolled + enrolled, "go"), (enrolled, "bad"),
                             (enrolled.replace("sos:", "***sos:"), "go")):
            with self.assertRaises(CopyError):
                verify_interactive_enrollment(output, mode)

    def test_interactive_lookup_and_logout_require_unique_originated_replies(self):
        lookup = "lookup fp\r\nfp =set 18 disc 0 0 0 0 0 ; system\r\n\x07"
        logout = "finis\r\n08.00 sos: test removed after finis\r\n"
        verify_interactive_lookup(lookup)
        verify_interactive_logout(logout)
        for bad in ("lookup fp\r\n", lookup + lookup, lookup.replace("18", "17"),
                    lookup.replace("disc", "disc1"), lookup + "***lookup"):
            with self.assertRaises(CopyError):
                verify_interactive_lookup(bad)
        for bad in ("finis\r\n", logout + logout, logout.replace("sos:", "primo:"),
                    logout.replace("after finis", "after break"), logout + "***error"):
            with self.assertRaises(CopyError):
                verify_interactive_logout(bad)

    def test_interactive_child_guard_is_separate_from_batch_running_state(self):
        listing = ("sos 72780 300000 5 0 wait s\r\n"
                   "test 77324 30208 3 0 stop sos\r\n")
        verify_interactive_child(listing)
        for bad in ("", listing + listing, listing.replace("3 0", "3 1"),
                    listing.replace("30208", "30000"), listing.replace("stop sos", "stop s"),
                    listing.replace("77324", "1"), listing.replace("77324", "400000")):
            with self.assertRaises(CopyError):
                verify_interactive_child(bad)

    def test_interactive_message_requires_output_not_just_echo(self):
        echo = "message termprobe\r\n"
        verify_interactive_message(echo + " termprobe\r\n\x07")
        for bad in (echo, echo + "***fp syntax\r\n", " termprobe\r\n",
                    echo + " termprobe\r\n termprobe\r\n"):
            with self.assertRaises(CopyError):
                verify_interactive_message(bad)

    def test_interactive_mode_rejects_mixed_or_unknown_dialogues_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "unused"
            for mode, probe, commands in (("bad", "terminal", []), ("go", "account", []),
                                          ("run", "terminal", ["display"])):
                with self.assertRaises(CopyError):
                    inspect(Path("unused"), destination, probe=probe,
                            interactive_terminal=mode, operator_commands=commands)
                self.assertFalse(destination.exists())

    def test_user_quota_catalog_matches_all_cumulative_claim_words(self):
        for builder, verifier, end in ((account_catalog_bytes, verify_account_catalog, 58),
                                       (terminal_catalog_bytes, verify_terminal_catalog, 60)):
            temporary, retained = builder(30), builder(30, True)
            self.assertEqual([i for i, (a, b) in enumerate(zip(temporary, retained)) if a != b],
                             [902, 905, 908, 911, 914, 917])
            self.assertEqual(retained[900:918], bytes.fromhex("000002 000006") * 3)
            response, descriptor = f"end {end}\r\n", "soscat =set 2 disc 0 0 0 0 0 ; temp\r\n"
            verifier(response, descriptor, retained, 30, True)
            for offset in (902, 905, 908, 911, 914, 917):
                altered = bytearray(retained)
                altered[offset] ^= 1
                with self.assertRaises(CopyError):
                    verifier(response, descriptor, altered, 30, True)
            with self.assertRaises(CopyError):
                builder(0, True)
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "unused"
            with self.assertRaises(CopyError):
                inspect(Path("unused"), destination, probe="terminal", account_user_storage=True)
            self.assertFalse(destination.exists())

    def test_retained_copy_requires_scope_bases_text_and_every_logical_byte(self):
        descriptor = "batchproof =set 1 disc 0 0 0 0 0 ; user\r\n ; 1 2 3 0 0\r\n"
        text = BATCH_JOBS["retain"]
        packed = (text + b"\x19").ljust(768, b"\0")
        verify_retained_batch_file(descriptor, text, packed, packed)
        for bad in ("", descriptor + descriptor, descriptor.replace("set 1", "set 2"),
                    descriptor.replace("disc", "disc1"), descriptor.replace("; user", "; temp"),
                    descriptor.replace("3 0 0", "3 1 1"), descriptor + "***lookup"):
            with self.assertRaises(CopyError):
                verify_retained_batch_file(bad, text, packed, packed)
        for data in (b"", text[:-1], text + b"\n"):
            with self.assertRaises(CopyError):
                verify_retained_batch_file(descriptor, data, packed, packed)
        for source, copy in ((b"", packed), (packed, b""), (packed, packed[:-1]),
                             (packed, packed + b"\0")):
            with self.assertRaises(CopyError):
                verify_retained_batch_file(descriptor, text, source, copy)
        for offset in (0, len(text) - 1, len(text)):
            altered = bytearray(packed)
            altered[offset] ^= 1
            with self.assertRaises(CopyError):
                verify_retained_batch_file(descriptor, text, packed, altered)
        padding = bytearray(packed)
        padding[-1] ^= 1
        verify_retained_batch_file(descriptor, text, packed, padding)
        with self.assertRaises(CopyError):
            verify_retained_batch_file(descriptor, text, bytes(768), bytes(768))

    def test_resource_catalog_changes_only_the_fixed_segment_word(self):
        for builder, verifier, end in ((account_catalog_bytes, verify_account_catalog, 58),
                                       (terminal_catalog_bytes, verify_terminal_catalog, 60)):
            baseline, resource = builder(), builder(30)
            self.assertEqual(len(resource), 1536)
            self.assertEqual([i for i, (a, b) in enumerate(zip(baseline, resource)) if a != b], [899])
            self.assertEqual(resource[897:900], bytes.fromhex("00001e"))
            response = f"end {end}\r\n"
            descriptor = "soscat =set 2 disc 0 0 0 0 0 ; temp\r\n"
            verifier(response, descriptor, resource, 30)
            for bad in (baseline, resource[:899] + b"\x1f" + resource[900:]):
                with self.assertRaises(CopyError):
                    verifier(response, descriptor, bad, 30)
            for bad in (response + response, response + "end 99\r\n", response + "errors 0", response.replace(str(end), "99")):
                with self.assertRaises(CopyError):
                    verifier(bad, descriptor, resource, 30)
            with self.assertRaises(CopyError):
                builder(31)

    def test_resource_inputs_fail_before_workspace_creation_when_not_fixed(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "unused"
            for probe, segments, text in (("catalog", 30, "maxp 4"), ("terminal", 31, "maxp 4"),
                                          ("account", 30, "maxp 4")):
                with self.assertRaises(CopyError):
                    inspect(Path("unused"), destination, probe=probe,
                            account_segments=segments, catalog_text=text)
                self.assertFalse(destination.exists())

    def test_batch_roundtrip_requires_both_matching_copy_summaries(self):
        data = BATCH_JOBS["roundtrip"]
        reports = "".join(f"{name} segm. 1 {len(data)}/{sum(data)}\r\n" for name in ("batchjob", "batchproof"))
        listing = ("tem 44842 11000 1 0 wait s\r\n"
                   "primo 55842 16938 2 0 wait s\r\n"
                   "sos 72780 300000 5 0 wait s\r\n")
        start, end = "08.00 sos: test enrolled\r\n", "08.00 sos: test removed after finis\r\n"
        verify_batch_job(start + reports + end, listing, "roundtrip")
        for bad in ("", reports + reports, reports.replace("batchproof", "other"),
                    reports.replace(f"{len(data)}/", "1/"), reports.replace(f"/{sum(data)}", "/0")):
            with self.assertRaises(CopyError):
                verify_batch_job(start + bad + end, listing, "roundtrip")

    def test_batch_message_and_lookup_require_actual_unique_output(self):
        enrolled = "08.00 sos: test enrolled\r\n"
        finished = "08.00 sos: test removed after finis\r\n"
        listing = ("tem 44842 11000 1 0 wait s\r\n"
                   "primo 55842 16938 2 0 wait s\r\n"
                   "sos 72780 300000 5 0 wait s\r\n")
        for workload, proof in (("message", " batchprobe\r\n"),
                                ("lookup", "batchjob =set 1 disc 0 0 0 0 0 ; system\r\n")):
            verify_batch_job(enrolled + proof + finished, listing, workload)
            for bad in ("", proof + proof, proof.replace("batch", "other")):
                with self.assertRaises(CopyError):
                    verify_batch_job(enrolled + bad + finished, listing, workload)
        with self.assertRaises(CopyError):
            verify_batch_job(enrolled + finished, listing, "arbitrary")

    def test_minimal_batch_requires_ordered_normal_finish_removed_child_and_healthy_services(self):
        enrolled = "08.00 sos: test enrolled\r\n"
        finished = "08.00 sos: test removed after finis\r\n"
        listing = ("tem 44842 11000 1 0 wait s\r\n"
                   "primo 55842 16938 2 0 wait s\r\n"
                   "sos 72780 300000 5 0 wait s\r\n")
        verify_batch_job(enrolled + finished, listing)
        for output in ("", enrolled, finished, finished + enrolled,
                       enrolled + enrolled + finished, enrolled + finished + finished,
                       enrolled + finished.replace("finis", "break"),
                       enrolled + "***break 10 1\r\n" + finished):
            with self.assertRaises(CopyError):
                verify_batch_job(output, listing)
        for bad in ("", listing.replace("5 0", "5 1"),
                    listing.replace("5 0", "6 0"), listing + listing,
                    listing + "test 77324 30208 3 0 run sos\r\n"):
            with self.assertRaises(CopyError):
                verify_batch_job(enrolled + finished, bad)

    def test_batch_job_rejects_other_commands_passwords_or_catalogs_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "unused"
            for probe, commands, password, lifecycle in (("sos", ["batch test jobfile batchjob"], "test", False),
                    ("terminal", ["batch test"], "test", False),
                    ("account", ["batch test jobfile batchjob"], "wrong", False),
                    ("terminal", ["batch test jobfile batchjob"], "test", True)):
                with self.assertRaises(CopyError):
                    inspect(Path("unused"), destination, probe=probe, operator_commands=commands,
                            test_password=password, verify_batch=lifecycle, batch_job="finis")
                self.assertFalse(destination.exists())

    def test_batch_snapshot_requires_unique_child_with_containing_sos_parent(self):
        core = bytearray(524288 * 3)
        def word(address, value):
            offset = address // 2 * 3
            core[offset:offset + 3] = value.to_bytes(3, "big")
        def descriptor(address, name, first, top):
            offset = (address + 2) // 2 * 3
            core[offset:offset + 12] = name.ljust(12, b"\0")
            word(address + 22, first)
            word(address + 24, top)
        descriptor(1000, b"sos", 70000, 370000)
        descriptor(1200, b"test", 75000, 105208)
        word(1250, 1000)
        word(1210, 0xb8)
        word(1232, 3)
        word(1248, 75664)
        state = batch_core_state(core)
        self.assertEqual((state["state"], state["stop_count"], state["exceptions"], state["key"]),
                         (0xb8, 0, 0, 3))
        with self.assertRaises(CopyError):
            batch_core_state(core[:-3])
        for field, invalid, original in ((1250, 999, 1000), (1224, 105206, 105208),
                                         (1000, 1, 0), (1024, 369998, 370000)):
            word(field, invalid)
            with self.assertRaises(CopyError):
                batch_core_state(core)
            word(field, original)
        descriptor(1400, b"test", 75000, 105208)
        word(1450, 1000)
        with self.assertRaises(CopyError):
            batch_core_state(core)

    def test_batch_verification_rejects_unfixed_inputs_before_creating_workspace(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "unused"
            for probe, commands, password in (("sos", ["batch test"], "test"),
                                              ("terminal", ["batch other"], "test"),
                                              ("account", ["batch test"], "wrong"),
                                              ("terminal", [], None)):
                with self.assertRaises(CopyError):
                    inspect(Path("unused"), destination, probe=probe,
                            operator_commands=commands, test_password=password, verify_batch=True)
                self.assertFalse(destination.exists())

    def test_batch_enrollment_requires_child_and_parent_stop_semantics(self):
        reply = "07.59 sos: test enrolled\r\n"
        listing = ("tem 44842 11000 1 0 wait s\r\n"
                   "primo 55842 16938 2 0 wait s\r\n"
                   "sos 72780 300000 5 1 wait s\r\n"
                   "test 77324 30208 3 0 run sos\r\n")
        verify_batch_enrollment(reply, listing)
        verify_batch_enrollment(reply, listing.replace("run sos", "wait sos"))
        for bad in ("", reply + reply, reply.replace("test enrolled", "ready"),
                    reply.replace("sos:", "***sos:")):
            with self.assertRaises(CopyError):
                verify_batch_enrollment(bad, listing)
        for bad in ("", listing + listing, listing.replace("5 1", "5 0"),
                    listing.replace("3 0", "3 1"), listing.replace("30208", "30000"),
                    listing.replace("run sos", "run s"), listing.replace("77324", "1"),
                    listing.replace("77324", "400000"), listing.replace("test ", "other "),
                    listing + "test 77324 30208 3 1 wait sos\r\n"):
            with self.assertRaises(CopyError):
                verify_batch_enrollment(reply, bad)

    def test_sos_second_disk_requires_exact_swap_descriptor(self):
        descriptor = "swpsos =set 1722 disc1 0 0 0 0 0 ; temp\r\n"
        verify_sos_disc1_storage(descriptor)
        for bad in ("", descriptor + descriptor, descriptor.replace("1722", "1721"),
                    descriptor.replace("disc1", "disc"), descriptor.replace("temp", "user"),
                    descriptor.replace("swpsos", "other"), descriptor + "***lookup"):
            with self.assertRaises(CopyError):
                verify_sos_disc1_storage(bad)

    def test_second_disk_retained_file_requires_document_size_scope_and_exact_payload(self):
        descriptor = f"dkeep =set {SECOND_DISC_SEGMENTS} disc1 d.860930.0759 0 0 0 0 ; user\r\n"
        self.assertGreater(len(SECOND_DISC_TEXT), 180 * 768)
        verify_second_disc_file(descriptor, SECOND_DISC_TEXT)
        for bad in ("", descriptor + descriptor, descriptor.replace("disc1", "disc"),
                    descriptor.replace("; user", "; temp"),
                    descriptor.replace(f"set {SECOND_DISC_SEGMENTS}", "set 1"),
                    descriptor + "***lookup"):
            with self.assertRaises(CopyError):
                verify_second_disc_file(bad, SECOND_DISC_TEXT)
        for data in (b"", SECOND_DISC_TEXT[:-1], SECOND_DISC_TEXT + b"\0"):
            with self.assertRaises(CopyError):
                verify_second_disc_file(descriptor, data)
        for offset in (0, 180 * 768, len(SECOND_DISC_TEXT) - 1):
            altered = bytearray(SECOND_DISC_TEXT)
            altered[offset] ^= 1
            with self.assertRaises(CopyError):
                verify_second_disc_file(descriptor, altered)

    def test_second_disk_requires_clean_utilities_mount_and_space(self):
        label = "kitlabel 16 disc1 catdisc1 slow 36 9 2030\r\nend 35\r\n\x07"
        mount = "kiton devno.16\r\ndisc1 mounted on 16\r\nend 48\r\n\x07"
        space = "show\r\nfree disc space:\r\ndisc1 : 2000 slices, 18000 segm.\r\n\x07"
        verify_second_disc(label, mount, space)
        for bad in ("", label + label, label + "giveup 0 algolcheck", label + "hard error"):
            with self.assertRaises(CopyError):
                verify_second_disc(bad, mount, space)
        for bad in ("", mount.replace("on 16", "on 15"), mount + "***kiton", mount + mount):
            with self.assertRaises(CopyError):
                verify_second_disc(label, bad, space)
        with self.assertRaises(CopyError):
            verify_second_disc(label, mount, space.replace("disc1", "disc"))

    def test_sos_dialogue_distinguishes_password_and_complete_response(self):
        question = b"batch test\r\n>password \x07"
        response = b"test\r\n07.59 sos: test enrolled\r\n"
        self.assertEqual(sos_reply_event(question), (len(question), "password"))
        self.assertEqual(sos_reply_event(response), (len(response), "response"))
        self.assertEqual(sos_reply_event(question + response), (len(question), "password"))
        self.assertEqual(sos_reply_event(response + question), (len(response), "response"))
        for data in (b"", question[:-1], response[:-1], response.replace(b"sos:", b"primo:")):
            self.assertIsNone(sos_reply_event(data))

    def test_terminal_fixture_requires_complete_record_and_normal_completion(self):
        response = "soscat=upsoscat catspec\r\n\r\nend 58\r\n\x07"
        descriptor = "soscat =set 2 disc 0 0 0 0 0 ; temp\r\n"
        data = terminal_catalog_bytes()
        self.assertEqual(len(data), 1536)
        self.assertEqual(data[1314:1326], b"console1" + bytes(4))
        self.assertEqual(data[1326:1329], b"1\0\0")
        self.assertEqual(data[1329:1341], b"test" + bytes(8))
        self.assertEqual(data[1341:1344], bytes.fromhex("03c002"))
        verify_terminal_catalog(response, descriptor, data)
        for offset in (0, 789, 801, 804, 1314, 1325, 1326, 1329, 1341, 1342, 1343, 1350, 1353, 1535):
            altered = bytearray(data)
            altered[offset] ^= 1
            with self.assertRaises(CopyError):
                verify_terminal_catalog(response, descriptor, altered)
        for bad in ("", response + response, response + "errors 0", response + "abnormal end",
                    response.replace("58", "56")):
            with self.assertRaises(CopyError):
                verify_terminal_catalog(bad, descriptor, data)
        with self.assertRaises(CopyError):
            verify_terminal_catalog(response, descriptor.replace("set 2", "set 1"), data)

    def test_parser_breakpoint_requires_known_unique_word_aligned_code(self):
        program = bytes(PARSER_OFFSET) + PARSER_SIGNATURE
        self.assertEqual(locate_parser_code(bytes(6) + PARSER_SIGNATURE, program), 4)
        for core in (b"", bytes(60), b"x" + PARSER_SIGNATURE,
                     PARSER_SIGNATURE + PARSER_SIGNATURE):
            with self.assertRaises(CopyError):
                locate_parser_code(core, program)
        for bad in (b"", program[:-1], program[:-1] + b"\0"):
            with self.assertRaises(CopyError):
                locate_parser_code(PARSER_SIGNATURE, bad)

    def test_catalog_outcome_does_not_promote_residual_area_after_rejection(self):
        descriptor = "soscat =set 2 disc 0 0 0 0 0 ; temp\r\n"
        response = ("name                         process test\r\n"
                    "abnormal end                 process test\r\nerrors 0 line 2024-2025\r\n")
        result = catalog_outcome(response, descriptor, bytes(1536))
        self.assertEqual(result["status"], "rejected")
        self.assertEqual(result["diagnostics"], ["name", "abnormal end"])
        self.assertTrue(result["export_matches_allocation"])
        self.assertFalse(result["account_login_verified"])
        self.assertFalse(result["terminal_connection_verified"])
        self.assertEqual(catalog_outcome(response + "end 56\n", descriptor, bytes(1536))["status"], "rejected")

    def test_catalog_outcome_distinguishes_normal_incomplete_and_inconsistent(self):
        descriptor = "soscat =set 2 disc 0 0 0 0 0 ; temp\r\n"
        response = "soscat=upsoscat catspec\r\n\r\nend 56\r\n\x07"
        self.assertEqual(catalog_outcome(response, descriptor, bytes(1536))["status"], "normal")
        for bad_descriptor, data in (("", None), (descriptor, bytes(768)),
                                     (descriptor + descriptor, bytes(1536)),
                                     (descriptor.replace("set 2", "set 0"), b"")):
            self.assertEqual(catalog_outcome(response, bad_descriptor, data)["status"], "inconsistent")
        self.assertEqual(catalog_outcome(response + response, descriptor, bytes(1536))["status"], "inconsistent")
        self.assertEqual(catalog_outcome("", "", None)["status"], "incomplete")

    def test_operator_commands_are_finite_printable_lines_in_sos_probes(self):
        validate_operator_commands("account", ["display", "call test"])
        validate_operator_commands("terminal", ["start"])
        validate_operator_commands("sos", ["x" * 79] * 8)
        validate_operator_commands("catalog", [])
        for probe, commands in (("catalog", ["display"]), ("account", [""]),
                                ("sos", ["x" * 80]), ("sos", ["x"] * 9),
                                ("sos", ["start\ncall test"]), ("sos", ["\x1b"]),
                                ("sos", ["\x7f"]), ("sos", ["\u00e4"])):
            with self.assertRaises(CopyError):
                validate_operator_commands(probe, commands)

    def test_operator_frame_requires_selection_echo_and_complete_timestamp(self):
        selection = "sos\r\n\r\n>\x07"
        reply = "display\r\n07.59 ***sos: command unknown\r\n"
        self.assertEqual(verify_operator_frame(selection, reply, "display"), "command unknown")
        self.assertEqual(verify_operator_frame(selection, "start\r\n07.59 sos: ready\r\n", "start"), "ready")
        for bad in ("", reply + reply, reply.replace("07.59", "bad"),
                    reply.replace("sos:", "primo:"), reply.rstrip(),
                    reply.replace("display", "other")):
            with self.assertRaises(CopyError):
                verify_operator_frame(selection, bad, "display")
        with self.assertRaises(CopyError):
            verify_operator_frame("sos\r\n", reply, "display")

    def test_account_fixture_requires_clean_completion_and_exact_record(self):
        response = "soscat=upsoscat catspec\r\n\r\nend 56\r\n\x07"
        descriptor = "soscat =set 2 disc 0 0 0 0 0 ; temp\r\n"
        data = account_catalog_bytes()
        self.assertEqual(len(data), 1536)
        self.assertEqual(data[801:807], bytes.fromhex("004e20 007530"))
        verify_account_catalog(response, descriptor, data)
        for offset in (0, 14, 759, 768, 789, 801, 804, 882, 894, 1314, 1535):
            altered = bytearray(data)
            altered[offset] ^= 1
            with self.assertRaises(CopyError):
                verify_account_catalog(response, descriptor, altered)
        for bad in ("", response + response, response + "errors 0", response + "abnormal end",
                    response.replace("56", "55")):
            with self.assertRaises(CopyError):
                verify_account_catalog(bad, descriptor, data)
        with self.assertRaises(CopyError):
            verify_account_catalog(response, descriptor.replace("set 2", "set 1"), data)

    def test_sos_requires_started_without_init_failure_and_all_active_rows(self):
        output = "07.59 sos: message sos release: 5.0 860901 930626\n07.59 sos: message sos started\n"
        listing = ("tem 44842 11000 1 0 wait s\nprimo 55842 16938 2 0 wait s\n"
                   "sos 72780 300000 5 0 wait s\n")
        verify_sos_startup(output, listing)
        for bad in ("", output + output, output + "***init troubles", output + "bs claims exceeded",
                    output.replace("release: 5.0", "release: other")):
            with self.assertRaises(CopyError):
                verify_sos_startup(bad, listing)
        with self.assertRaises(CopyError):
            verify_sos_startup(output, listing.replace("0 wait s", "1 error s"))

    def test_primo_success_and_unknown_command_are_distinct_completions(self):
        success = "display\r\n07.59 primo:ready\r\n"
        rejected = "zzzzz\r\n07.59 primo:***command unknown\r\n"
        verify_primo_reply(success)
        verify_primo_reply(rejected, "command unknown")
        for bad in ("", success + success, rejected, success.replace("primo", "other")):
            with self.assertRaises(CopyError):
                verify_primo_reply(bad)
        for bad in (success, rejected + success, rejected + rejected, rejected.replace("unknown", "bad")):
            with self.assertRaises(CopyError):
                verify_primo_reply(bad, "command unknown")

    def test_empty_catalog_requires_normal_end_descriptor_and_exact_footer(self):
        response = "soscat=upsoscat catspec\r\n\r\nend 42\r\n\x07"
        descriptor = "soscat =set 1 disc 0 0 0 0 0 ; temp\r\n"
        verify_empty_catalog(response, descriptor, EMPTY_CATALOG)
        for bad in (b"\xff" * 768, EMPTY_CATALOG[:-1], bytes(768)):
            with self.assertRaises(CopyError):
                verify_empty_catalog(response, descriptor, bad)
        for bad in (response.replace("42", "41"), response + "errors 0", response + "abnormal end"):
            with self.assertRaises(CopyError):
                verify_empty_catalog(bad, descriptor, EMPTY_CATALOG)
        with self.assertRaises(CopyError):
            verify_empty_catalog(response, descriptor.replace("set 1", "set 2"), EMPTY_CATALOG)

    def test_private_startup_changes_exactly_one_valid_resource_clause(self):
        source = b"* new sos size 300000\n* perm disc 2500 200 prog bsos\n* run\n"
        self.assertEqual(private_sos_start(source, 300, 8), source.replace(b"2500 200", b"300 8"))
        self.assertEqual(private_sos_start(source, 2500, 200, "disc1"),
                         source.replace(b"perm disc ", b"perm disc1 "))
        for document in ("", "disc2", "disc1\nrun", "disc1 1 2"):
            with self.assertRaises(CopyError):
                private_sos_start(source, 2500, 200, document)
        for bad in (b"", source + source, source.replace(b"2500", b"2501")):
            with self.assertRaises(CopyError):
                private_sos_start(bad, 300, 8)
        for segments, entries in ((0, 8), (300, 0), (6091, 8), (300, 1081)):
            with self.assertRaises(CopyError):
                private_sos_start(source, segments, entries)

    def test_system_reply_waits_for_ready_and_rejects_errors_without_timeout(self):
        class Lines:
            def __init__(self, lines):
                self.lines = iter(lines)

            def until(self, marker, timeout):
                assert marker == b"\n" and 0 < timeout <= 30
                return next(self.lines)

        lines = ["max\r\n", "max 100000 20 20 5 5\r\n", "ready  \r\n"]
        recorded = []
        self.assertEqual(read_system_reply(Lines(lines), lambda line: recorded.append(line) or line), "".join(lines))
        self.assertEqual(recorded, lines)
        for error in ("bs claims exceeded", "area unknown", "syntax error: 0", "to sos"):
            with self.assertRaises(CopyError):
                read_system_reply(Lines([error + "\r\n"]), lambda line: line)
        with self.assertRaises(CopyError):
            read_system_reply(Lines(["noise\n"] * 200), lambda line: line)

    def test_existing_work_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                inspect(Path("unused-builder"), directory)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_inventory_requires_clean_text_binary_sizes_and_completion(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            exchange = directory / "exchange"
            exchange.mkdir()
            good = {**{name: b"* unstack\n" for name in TEXTS},
                    **{name: bytes(size * 768) for name, size in BINARIES.items()},
                    "metadata": b"startmips =set 1 disc 0 ; system\n"}
            for name, data in good.items():
                (exchange / name).write_bytes(data)
            log = directory / "inventory.log"
            success = "MIPS inventory exported without executing subsystem startup\nresult=0\n"
            log.write_text(success, encoding="ascii")
            verify_inventory(directory)
            for name, data in good.items():
                (exchange / name).write_bytes(b"\0")
                with self.assertRaises(CopyError):
                    verify_inventory(directory)
                (exchange / name).unlink()
                with self.assertRaises(FileNotFoundError):
                    verify_inventory(directory)
                (exchange / name).write_bytes(data)
            for bad in ("", success + success, success + "***fp error\n",
                        success.replace("result=0", "result=1"), success + "syntax error:\n"):
                log.write_text(bad, encoding="ascii")
                with self.assertRaises(CopyError):
                    verify_inventory(directory)

    def test_component_rows_require_active_unique_matching_processes(self):
        good = ("tem 44842 11000 1 0 wait s\r\n"
                "primo 55842 16938 2 0 run s\r\nready\r\n")
        verify_component_listing(good, ("tem", "primo"))
        for old, new in (("11000", "11002"), ("16938", "16936"),
                         ("1 0 wait", "1 1 stop"), ("1 0 wait", "0 0 wait"),
                         ("wait s", "wait other"), ("44842", "0"),
                         ("2 0 run", "2 0 bad")):
            with self.assertRaises(CopyError):
                verify_component_listing(good.replace(old, new), ("tem", "primo"))
        for bad in ("", good + good):
            with self.assertRaises(CopyError):
                verify_component_listing(bad, ("tem", "primo"))


if __name__ == "__main__":
    unittest.main()
