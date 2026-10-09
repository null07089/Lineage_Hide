#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
#
# lineage-hide-patch-kernel.sh - hide custom ROM names from procfs path output.
#
# The change rewrites ROM identifiers ("lineage", "crdroid", ...) in the path
# strings that procfs prints to application processes (uid >= 10000):
#   - fs/proc/base.c: do_proc_readlink()  -> /proc/<pid>/fd/*, map_files/*
#   - fs/seq_file.c:  seq_file_path()     -> /proc/<pid>/maps, smaps, numa_maps
# Only the display string is rewritten in place; file access is not affected.
#
# The patcher is semantic (anchors on functions, not line numbers) and
# idempotent, so it also works on trees with vendor or SUSFS modifications.
# Edited files are backed up next to the originals as *.lineage_hide.bak.
#
# Usage:
#   scripts/patch-kernel.sh [--check|--revert] <kernel-dir>
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: patch-kernel.sh [--check|--revert] <kernel-dir>

  (no option)  apply the procfs ROM-name scrub and write the header
  --check      report whether the patch is fully applied; no changes
  --revert     remove the patch and the header

Examples:
  scripts/patch-kernel.sh /root/android_kernel_oneplus_sm8250
  scripts/patch-kernel.sh --check /path/to/kernel
  scripts/patch-kernel.sh --revert /path/to/kernel

Rebuild and flash the kernel after applying.  Backups are written next to the
edited files as *.lineage_hide.bak.
EOF
}

MODE=apply
KDIR=
while [ $# -gt 0 ]; do
    case "$1" in
        --check) MODE=check ;;
        --revert) MODE=revert ;;
        -h|--help) usage; exit 0 ;;
        --*) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
        *)
            if [ -n "$KDIR" ]; then
                echo "error: only one kernel directory may be given" >&2
                exit 2
            fi
            KDIR=$1
            ;;
    esac
    shift
done

if [ -z "$KDIR" ]; then
    usage >&2
    exit 2
fi
if [ ! -f "$KDIR/Makefile" ] || [ ! -d "$KDIR/include/linux" ]; then
    echo "error: '$KDIR' does not look like a kernel source tree" >&2
    exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 is required to apply the patch" >&2
    exit 1
fi

python3 - "$MODE" "$KDIR" <<'PY'
import os
import re
import shutil
import sys

MODE = sys.argv[1]
KDIR = sys.argv[2].rstrip("/")

BASE = os.path.join(KDIR, "fs/proc/base.c")
SEQ = os.path.join(KDIR, "fs/seq_file.c")
HEADER = os.path.join(KDIR, "include/linux/lineage_hide.h")

HEADER_TEXT = """/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Scrub custom ROM identifiers out of path strings that procfs shows to
 * application processes.  Only the display string is rewritten in place;
 * file access is not affected.
 */
#ifndef _LINUX_YUKARI_HIDE_H
#define _LINUX_YUKARI_HIDE_H

#include <linux/cred.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>

#define LINEAGE_HIDE_APP_UID_MIN 10000

static inline bool lineage_hide_active(void)
{
\treturn current_uid().val >= LINEAGE_HIDE_APP_UID_MIN;
}

static inline void lineage_hide_scrub_rom_names(char *path)
{
\tstatic const char *const rom_names[] = {
\t\t"lineage", "crdroid", "aospa", "pixelexperience",
\t\t"omnirom", "protonaosp",
\t};
\tsize_t i, len;
\tchar *p;

\tif (!path || !lineage_hide_active())
\t\treturn;

\tfor (i = 0; i < ARRAY_SIZE(rom_names); i++) {
\t\tlen = strlen(rom_names[i]);
\t\tfor (p = path; *p; p++) {
\t\t\tif (!strncasecmp(p, rom_names[i], len))
\t\t\t\tp[len - 1] = 'x';
\t\t}
\t}
}

#endif /* _LINUX_YUKARI_HIDE_H */
"""

BASE_INCLUDE = "#include <linux/lineage_hide.h>"
BASE_CALL = "lineage_hide_scrub_rom_names(pathname);"
BASE_ANCHOR = "\tlen = tmp + PAGE_SIZE - 1 - pathname;\n"
SEQ_INCLUDE = "#include <linux/lineage_hide.h>"
SEQ_SIG = "int seq_file_path(struct seq_file *m, struct file *file, const char *esc)"
SEQ_ORIGINAL = "\treturn seq_path(m, &file->f_path, esc);\n"
SEQ_PATCHED = (
    "\tif (lineage_hide_active()) {\n"
    "\t\tchar *buf;\n"
    "\t\tsize_t size = seq_get_buf(m, &buf);\n"
    "\t\tint res = -1;\n"
    "\n"
    "\t\tif (size) {\n"
    "\t\t\tchar *p = d_path(&file->f_path, buf, size);\n"
    "\n"
    "\t\t\tif (!IS_ERR(p)) {\n"
    "\t\t\t\tchar *end;\n"
    "\n"
    "\t\t\t\tlineage_hide_scrub_rom_names(p);\n"
    "\t\t\t\tend = mangle_path(buf, p, esc);\n"
    "\t\t\t\tif (end)\n"
    "\t\t\t\t\tres = end - buf;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tseq_commit(m, res);\n"
    "\n"
    "\t\treturn res;\n"
    "\t}\n"
    "\n"
    "\treturn seq_path(m, &file->f_path, esc);\n"
)


def read(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as fh:
        return fh.read()


def backup(path):
    bak = path + ".lineage_hide.bak"
    if os.path.exists(path) and not os.path.exists(bak):
        shutil.copy2(path, bak)


def write(path, text):
    backup(path)
    tmp = path + ".lineage_hide.tmp"
    with open(tmp, "w", encoding="utf-8", errors="surrogateescape") as fh:
        fh.write(text)
    os.replace(tmp, path)


def add_include(text, include, anchor=None):
    if include in text:
        return text
    if anchor and text.count(anchor) == 1:
        return text.replace(anchor, anchor + include + "\n", 1)
    match = re.search(r"^#include[^\n]*\n", text, re.M)
    if not match:
        raise RuntimeError("cannot find the include block")
    return text[: match.end()] + include + "\n" + text[match.end():]


def remove_include(text, include):
    return "".join(
        line for line in text.splitlines(keepends=True) if line.strip() != include
    )


def find_function(text, name):
    match = re.search(
        r"^[A-Za-z_][^\n]*\b" + re.escape(name) + r"\s*\([^\n]*\)\n\{", text, re.M
    )
    if not match:
        return None
    index = match.end() - 1
    depth = 0
    while index < len(text):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return match.start(), index + 1
        index += 1
    return None


def find_seq_function(text):
    return re.search(
        r"(" + re.escape(SEQ_SIG) + r"\n\{\n)(.*?)(\n\}\n)", text, re.S
    )


def plan_base_apply(text):
    if BASE_CALL in text and BASE_INCLUDE in text:
        return text, "already applied"
    if BASE_CALL in text or BASE_INCLUDE in text:
        raise RuntimeError(
            "fs/proc/base.c looks partially patched; clean or revert it first"
        )
    text = add_include(text, BASE_INCLUDE, '#include "internal.h"\n')
    if text.count(BASE_ANCHOR) == 1:
        text = text.replace(BASE_ANCHOR, "\t" + BASE_CALL + "\n" + BASE_ANCHOR, 1)
        return text, "patched"
    span = find_function(text, "do_proc_readlink")
    if not span:
        raise RuntimeError(
            "do_proc_readlink() not found; this kernel layout is not supported"
        )
    start, end = span
    body = text[start:end]
    match = re.search(r"^\tif \(len > buflen\)\n", body, re.M)
    if not match:
        raise RuntimeError("cannot find an insertion point inside do_proc_readlink()")
    position = start + match.start()
    text = text[:position] + "\t" + BASE_CALL + "\n" + text[position:]
    return text, "patched"


def plan_base_revert(text):
    if BASE_CALL not in text and BASE_INCLUDE not in text:
        return text, "not applied"
    lines = [
        line
        for line in text.splitlines(keepends=True)
        if line.strip() not in (BASE_CALL, BASE_INCLUDE)
    ]
    return "".join(lines), "reverted"


def plan_seq_apply(text):
    if "lineage_hide_active" in text and SEQ_INCLUDE in text:
        return text, "already applied"
    if "lineage_hide_active" in text or SEQ_INCLUDE in text:
        raise RuntimeError(
            "fs/seq_file.c looks partially patched; clean or revert it first"
        )
    text = add_include(text, SEQ_INCLUDE, "#include <linux/seq_file.h>\n")
    match = find_seq_function(text)
    if not match:
        raise RuntimeError("seq_file_path() not found; this kernel layout is not supported")
    if match.group(2).strip() != "return seq_path(m, &file->f_path, esc);":
        raise RuntimeError("seq_file_path() has an unexpected body; patch it manually")
    text = (
        text[: match.start()]
        + SEQ_SIG
        + "\n{\n"
        + SEQ_PATCHED
        + "}\n"
        + text[match.end():]
    )
    return text, "patched"


def plan_seq_revert(text):
    if "lineage_hide_active" not in text and SEQ_INCLUDE not in text:
        return text, "not applied"
    match = find_seq_function(text)
    if not match:
        raise RuntimeError("seq_file_path() not found while reverting")
    text = (
        text[: match.start()]
        + SEQ_SIG
        + "\n{\n"
        + SEQ_ORIGINAL
        + "}\n"
        + text[match.end():]
    )
    return remove_include(text, SEQ_INCLUDE), "reverted"


def header_state():
    if not os.path.exists(HEADER):
        return "missing"
    return "current" if read(HEADER) == HEADER_TEXT else "different"


results = []
try:
    for path in (BASE, SEQ):
        if not os.path.isfile(path):
            raise RuntimeError("%s is missing" % os.path.relpath(path, KDIR))
    base_text = read(BASE)
    seq_text = read(SEQ)

    if MODE == "apply":
        new_base, status_base = plan_base_apply(base_text)
        new_seq, status_seq = plan_seq_apply(seq_text)
        status_header = header_state()
        if (
            status_base == "already applied"
            and status_seq == "already applied"
            and status_header == "current"
        ):
            print("already applied; nothing to do")
            sys.exit(0)
        write(BASE, new_base)
        write(SEQ, new_seq)
        if status_header != "current":
            write(HEADER, HEADER_TEXT)
            status_header = "written"
        results = [
            ("fs/proc/base.c", status_base),
            ("fs/seq_file.c", status_seq),
            ("include/linux/lineage_hide.h", status_header),
        ]
    elif MODE == "revert":
        new_base, status_base = plan_base_revert(base_text)
        new_seq, status_seq = plan_seq_revert(seq_text)
        status_header = header_state()
        if (
            status_base == "not applied"
            and status_seq == "not applied"
            and status_header == "missing"
        ):
            print("not applied; nothing to do")
            sys.exit(0)
        if status_base == "reverted":
            write(BASE, new_base)
        if status_seq == "reverted":
            write(SEQ, new_seq)
        if status_header != "missing":
            os.remove(HEADER)
        results = [
            ("fs/proc/base.c", status_base),
            ("fs/seq_file.c", status_seq),
            ("include/linux/lineage_hide.h", "removed" if status_header != "missing" else "missing"),
        ]
    elif MODE == "check":
        _, status_base = plan_base_apply(base_text)
        _, status_seq = plan_seq_apply(seq_text)
        status_header = header_state()
        results = [
            ("fs/proc/base.c", status_base),
            ("fs/seq_file.c", status_seq),
            ("include/linux/lineage_hide.h", status_header),
        ]
        print("check %s" % KDIR)
        for name, status in results:
            print("  %-34s %s" % (name, status))
        if (
            status_base == "already applied"
            and status_seq == "already applied"
            and status_header == "current"
        ):
            print("result: fully applied")
            sys.exit(0)
        print("result: not fully applied")
        sys.exit(1)
    else:
        raise RuntimeError("unknown mode %s" % MODE)
except RuntimeError as exc:
    print("error: %s" % exc, file=sys.stderr)
    sys.exit(1)

print("%s %s" % (MODE, KDIR))
for name, status in results:
    print("  %-34s %s" % (name, status))
if MODE == "apply":
    print("rebuild the kernel and flash it; the Lineage Hide module needs no change")
PY
