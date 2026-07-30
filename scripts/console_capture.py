#!/usr/bin/env python3
"""Serial console capture (host half of the UART virtualization).

Opens the board serial port FIRST (so the firmware banner is never lost),
then launches the given command (typically the xsct JTAG run script), and
streams everything the board prints to stdout and to the output file until
the firmware's terminator line appears.

Python stdlib only (termios, no pyserial) so it also runs on remote board
hosts without nix.

Exit codes:
  0  terminator seen and firmware printed STATUS: OK
  2  terminator seen but firmware printed STATUS: FAIL (or no STATUS line)
  3  timeout waiting for the terminator
  4  serial port could not be opened
  5  launch command failed to start or exited nonzero before the terminator
"""

import argparse
import os
import select
import subprocess
import sys
import termios
import time

TERMINATOR = "Application finished"


def open_serial(port, baud):
    try:
        speed = getattr(termios, "B%d" % baud)
    except AttributeError:
        print("console_capture: unsupported baud rate %d" % baud, file=sys.stderr)
        sys.exit(4)

    try:
        fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    except OSError as exc:
        print("console_capture: cannot open %s: %s" % (port, exc), file=sys.stderr)
        print("console_capture: is the port free (stale screen session?) and "
              "are you in the dialout group?", file=sys.stderr)
        sys.exit(4)

    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, _, _, cc = attrs
    iflag = 0
    oflag = 0
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    cflag |= termios.CS8 | termios.CLOCAL | termios.CREAD
    if hasattr(termios, "CRTSCTS"):
        cflag &= ~termios.CRTSCTS
    lflag = 0
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW,
                      [iflag, oflag, cflag, lflag, speed, speed, cc])
    termios.tcflush(fd, termios.TCIFLUSH)
    return fd


def main():
    p = argparse.ArgumentParser()
    p.add_argument("-s", "--serial", required=True, help="serial port device")
    p.add_argument("-b", "--baud", type=int, default=115200)
    p.add_argument("-o", "--output", required=True, help="capture file")
    p.add_argument("-t", "--timeout", type=int, default=120,
                   help="seconds to wait for the terminator")
    p.add_argument("--terminator", default=TERMINATOR)
    p.add_argument("cmd", nargs=argparse.REMAINDER,
                   help="command to launch after the port is open "
                        "(prefix with --)")
    args = p.parse_args()

    cmd = args.cmd
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]

    fd = open_serial(args.serial, args.baud)
    print("console_capture: listening on %s @ %d" % (args.serial, args.baud))

    proc = None
    if cmd:
        print("console_capture: launching: %s" % " ".join(cmd))
        try:
            proc = subprocess.Popen(cmd)
        except OSError as exc:
            print("console_capture: launch failed: %s" % exc, file=sys.stderr)
            os.close(fd)
            sys.exit(5)

    deadline = time.time() + args.timeout
    text = ""
    status_ok = False
    status_seen = False
    finished = False

    outdir = os.path.dirname(os.path.abspath(args.output))
    os.makedirs(outdir, exist_ok=True)

    with open(args.output, "wb") as out:
        while time.time() < deadline:
            ready, _, _ = select.select([fd], [], [], 0.2)
            if ready:
                try:
                    data = os.read(fd, 4096)
                except OSError:
                    data = b""
                if data:
                    out.write(data)
                    out.flush()
                    sys.stdout.write(data.decode("utf-8", errors="replace"))
                    sys.stdout.flush()
                    text += data.decode("utf-8", errors="replace")
                    if "STATUS: OK" in text:
                        status_seen = True
                        status_ok = True
                    elif "STATUS: FAIL" in text:
                        status_seen = True
                    if args.terminator in text:
                        finished = True
                        break

            if proc is not None:
                rc = proc.poll()
                if rc is not None and rc != 0:
                    print("\nconsole_capture: launch command exited with %d "
                          "before the run finished" % rc, file=sys.stderr)
                    os.close(fd)
                    sys.exit(5)

    os.close(fd)
    if proc is not None and proc.poll() is None:
        proc.wait()

    if not finished:
        print("\nconsole_capture: TIMEOUT after %ds waiting for '%s'"
              % (args.timeout, args.terminator), file=sys.stderr)
        sys.exit(3)

    if status_ok:
        print("\nconsole_capture: run finished, STATUS: OK")
        sys.exit(0)

    print("\nconsole_capture: run finished, STATUS: %s"
          % ("FAIL" if status_seen else "missing"), file=sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
    main()
