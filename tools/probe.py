#!/usr/bin/env python3
"""probe.py <disk.d81> <seconds> [monitor command ...] — boot the disk in
xemu headless, wait, run monitor commands (r: registers, m<addr>: memory)
and print what they answer; then a screenshot to build/probe.png"""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screenshot import monitor  # noqa: E402

disk = os.path.abspath(sys.argv[1])
delay = float(sys.argv[2])
cmds = sys.argv[3:] or ["r"]
tmp = tempfile.mkdtemp()
sockname = os.path.join(tmp, "umon")
png = os.path.abspath("build/probe.png")
xemu = subprocess.Popen(
    [os.environ.get("XEMU", "xemu-xmega65"), "-headless", "-besure", "-fastboot", "-uartmon", sockname,
     "-screenshot", png, "-autoload", "-8", disk],
    stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    sock = None
    end = time.time() + 20
    while sock is None and time.time() < end:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(sockname)
            s.settimeout(0.3)
            sock = s
        except OSError:
            time.sleep(0.1)
    time.sleep(delay)
    for c in cmds:
        print(">", c)
        print(monitor(sock, c))
    sock.close()
    xemu.send_signal(signal.SIGTERM)
    xemu.wait(timeout=15)
finally:
    if xemu.poll() is None:
        xemu.kill()
