#!/usr/bin/env python3
"""Static guards for AyuGram invariants that no unit test can reach.

Both checks target the failure mode that survives a clean git merge: upstream
moves code and the fork's hook silently stops covering it.

Run from anywhere:  Telegram/SourceFiles/ayu/tests/guard_checks.py
Exits non-zero if any guard fails.
"""

import os
import re
import subprocess
import sys

ROOT = subprocess.run(
    ["git", "rev-parse", "--show-toplevel"],
    cwd=os.path.dirname(os.path.abspath(__file__)),
    capture_output=True, text=True, check=True).stdout.strip()
SRC = os.path.join(ROOT, "Telegram", "SourceFiles")

failures = []
checks = 0


def report(ok, what, detail=""):
    global checks
    checks += 1
    print(f"  {'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        if detail:
            print(f"        {detail}")
        failures.append(what)


def read(rel):
    with open(os.path.join(SRC, rel), encoding="utf-8", errors="ignore") as f:
        return f.read()


def sources(*roots, exts=(".cpp", ".h", ".mm")):
    for root in roots:
        for base, _, names in os.walk(os.path.join(SRC, root)):
            for n in names:
                if n.endswith(exts):
                    p = os.path.join(base, n)
                    yield os.path.relpath(p, SRC), p


def body_of(text, signature):
    m = re.search(re.escape(signature) + r".*?\n\}", text, re.S)
    return m.group(0) if m else ""


def struct_body(text, name):
    m = re.search(r"(?:class|struct)\s+" + re.escape(name) + r"\s*(?:final\s*)?\{", text)
    if not m:
        return ""
    i, depth = m.end(), 1
    while i < len(text) and depth:
        depth += (text[i] == "{") - (text[i] == "}")
        i += 1
    return text[m.end():i]


def check_settings_persistence():
    print("\n== every AyuSettings field survives a restart ==")
    header = read("ayu/ayu_settings.h")
    impl = read("ayu/ayu_settings.cpp")

    written = set(re.findall(r'\{"([A-Za-z0-9_]+)"',
                             body_of(impl, "void to_json(nlohmann::json &j, const AyuSettings &s)")))
    read_back = set(re.findall(r'"([A-Za-z0-9_]+)"',
                               body_of(impl, "void from_json(const nlohmann::json &j, AyuSettings &s)")))

    own = re.findall(r"rpl::variable<[^>]+>\s+_([A-Za-z0-9_]+)", struct_body(header, "AyuSettings"))
    report(bool(own), f"AyuSettings fields discovered ({len(own)})")

    lost = [f for f in own if f not in written]
    report(not lost, "no field is written but absent from to_json", ", ".join(lost))

    unread = [f for f in own if f not in read_back]
    report(not unread, "no field is missing from from_json", ", ".join(unread))

    # Nested settings own their serialisation; they must still be referenced.
    for nested, key in (("GhostModeAccountSettings", "ghostModeSettings"),
                        ("MessageShotSettings", "messageShotSettings")):
        fields = re.findall(r"rpl::variable<[^>]+>\s+_([A-Za-z0-9_]+)", struct_body(header, nested))
        report(bool(fields) and key in written and key in read_back,
               f"{nested} ({len(fields)} fields) is serialised via \"{key}\"")


GHOST_REQUESTS = {
    "MTPmessages_ReadHistory": "read history",
    "MTPchannels_ReadHistory": "read history",
    "MTPmessages_ReadMessageContents": "read media",
    "MTPchannels_ReadMessageContents": "read media",
    "MTPmessages_ReadDiscussion": "read discussion",
    "MTPaccount_UpdateStatus": "online status",
}

# Each request must be reachable only through a function that reads the
# matching ghost setting. Naming the funnel explicitly is the point: if
# upstream moves a request out of it, the request becomes unattributed and
# the last check below fails.
GHOST_FUNNELS = [
    ("data/data_histories.cpp", "Histories::sendReadRequests", "sendReadMessages",
     ["MTPmessages_ReadHistory", "MTPchannels_ReadHistory"],
     ["Histories::sendReadRequest"]),
    ("apiwrap.cpp", "ApiWrap::markContentsRead", "sendReadMessages",
     ["MTPmessages_ReadMessageContents", "MTPchannels_ReadMessageContents"],
     ["ApiWrap::markContentsRead"]),
    ("api/api_updates.cpp", "Updates::updateOnline", "sendOnlinePackets",
     ["MTPaccount_UpdateStatus"],
     ["Updates::updateOnline"]),
]


def strip_comments(text):
    """A guard named only in a comment is not a guard."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def function_bodies(text):
    """Split a .cpp into Qualified::name -> body using column-0 signatures."""
    lines = strip_comments(text).split("\n")
    marks = []
    for i, line in enumerate(lines):
        m = re.match(r"^[A-Za-z_].*?\b(\w+::\w+)\s*\(", line)
        if m:
            marks.append((i, m.group(1)))
    out = {}
    for idx, (start, name) in enumerate(marks):
        end = marks[idx + 1][0] if idx + 1 < len(marks) else len(lines)
        out.setdefault(name, []).append("\n".join(lines[start:end]))
    return out


def check_ghost_guard_coverage():
    print("\n== no read/online request escapes a ghost guard ==")
    attributed = set()

    for rel, guard_fn, setting, requests, senders in GHOST_FUNNELS:
        text = read(rel)
        bodies = function_bodies(text)

        guard_bodies = bodies.get(guard_fn, [])
        report(any(setting + "()" in b for b in guard_bodies),
               f"{guard_fn} reads {setting}()",
               f"the guard for {rel} is gone or renamed")

        for sender_fn in senders:
            for request in requests:
                where = [b for b in bodies.get(sender_fn, []) if request + "(" in b]
                if where:
                    attributed.add((rel, request))
        for request in requests:
            in_file = request + "(" in text
            if in_file:
                report((rel, request) in attributed,
                       f"{rel}: {request} sits inside {' / '.join(senders)}",
                       "the request moved out of the guarded funnel")

    # Nothing may send one of these requests from a file we have not vetted.
    known = {rel for rel, *_ in GHOST_FUNNELS}
    known.add("data/data_replies_list.cpp")  # read discussion, guarded in place
    stray = []
    for rel, path in sources("."):
        if rel.startswith("ayu/") or rel in known:
            continue
        text = open(path, encoding="utf-8", errors="ignore").read()
        for request in GHOST_REQUESTS:
            if request + "(" in text:
                stray.append(f"{rel}:{request}")
    report(not stray, "no presence-revealing request outside the vetted files",
           ", ".join(stray))


def check_storage_layer_style():
    print("\n== REVIEW.md rules hold in the storage layer ==")
    # std::optional::value() throws std::bad_optional_access, which is not
    # available on the older macOS targets this fork still ships for. The
    # storage layer is the part the unit tests compile and link for real, so a
    # violation here is the one that actually reaches a user's machine.
    offenders = []
    for rel, path in sources("ayu/data"):
        text = strip_comments(open(path, encoding="utf-8", errors="ignore").read())
        for number, line in enumerate(text.split("\n"), 1):
            if ".value()" in line:
                offenders.append(f"{rel}:{number}")
    report(not offenders, "ayu/data calls no std::optional::value()",
           ", ".join(offenders))


def main():
    print("AyuGram static guards")
    check_settings_persistence()
    check_ghost_guard_coverage()
    check_storage_layer_style()
    print(f"\n{checks - len(failures)} passed, {len(failures)} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
