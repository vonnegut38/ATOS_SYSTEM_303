#!/usr/bin/env python3
"""Boot check for the TOS build profiles.

TOS=0 (default) must boot the native text shell and report that the persona is
not built when the TOS command is used.  TOS=1 must boot it too, with the
persona present.  CPU selects the 68000 (0) or 68030 (3).

Exit status is non-zero when the expected text is missing.
"""
import os, sys, time

sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

PROJECT = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
CPU = os.environ.get("CPU", "3")
EXPECT_MISSING = os.environ.get("EXPECT_MISSING", "1") == "1"
h = Hatari(PROJECT, log=PROJECT + "/work/hatari-tos-profile-" + CPU + ".log",
           window=False, extra=["--cpulevel", CPU])


def cursor_line():
    lines = h.console_lines(rows=1) or []
    return lines[-1].rstrip() if lines else ""


def send_run(text):
    for _ in range(12):
        h.type(text, delay=0.16)
        for _ in range(14):
            time.sleep(0.25)
            if cursor_line().endswith(text):
                h.press("\n", delay=0.15)
                return True
        for _ in range(30):
            h.press(chr(8), 0.05)
        time.sleep(0.4)
    return False


try:
    h.wait(14)
    boot = "\n".join(h.console_lines(rows=24) or [])
    native = "ATOS 2.0: NATIVE TEXT SESSIONS" in boot
    # The TOS command must run before a foreground app takes the console: once
    # SYSINFO.APP is attached the shell stops printing a prompt.
    if not send_run("tos SYSINFO.APP"):
        print("could not type tos command", flush=True)
    time.sleep(3.0)
    tos = "\n".join(h.console_lines(rows=24) or [])
    missing = "TOS PERSONA NOT BUILT" in tos
    if not send_run("run SYSINFO.APP"):
        print("could not type run command", flush=True)
    time.sleep(3.0)
    app = "\n".join(h.console_lines(rows=24) or [])
    app_ok = "PROCESS READY" in app
    print("native_banner=%s app_started=%s persona_missing=%s" % (native, app_ok, missing), flush=True)
    ok = native and app_ok and (missing == EXPECT_MISSING)
    print("RESULT:", "OK" if ok else "FAIL")
    for line in tos.splitlines()[-12:]:
        print("  |" + line)
    sys.exit(0 if ok else 1)
finally:
    h.close()
