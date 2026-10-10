#!/usr/bin/env python3
"""
Regenerate runtime-config/example.cfg from the q2aCommands[] table.

example.cfg lists every option a config file can set, with the default it
takes when no config sets it, so it goes stale the moment a command is added,
renamed or removed. Rather than maintain it by hand, this reads the table in
g_cmd.c, resolves each default from the initializer of the variable the entry
points at, and writes the file out sorted and aligned.

It bails out rather than guessing. A new option whose default it cannot work
out is an error, not a blank value, so "make example.cfg" fails loudly and
the fix is to teach SPECIAL below about it.

It also reports any setting in runtime-config/*.cfg that matches no command.
Those lines are silently ignored by the real parser, which is how a typo like
"client_map_config" for "client_map_cfg" can sit unnoticed for years.

Usage: gen-example-cfg.py [output-path]
"""

import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
CFGDIR = ROOT / "runtime-config"
DEFAULT_OUT = CFGDIR / "example.cfg"

# Defaults that are not a literal in the variable's initializer. Anything that
# lands here needs a human decision, so each one says why.
SPECIAL = {
    # CLMAPCFG_EXACT BIT(1) | CLMAPCFG_ALL BIT(2).
    "client_map_cfg": "6",
    # sizeof(struct edict_s) + 20, so it depends on the mod's entity layout.
    # 300 is the value on a 64-bit build; see the warning in README.md, a wrong
    # value here can crash the server.
    "entity_classname_offset": "300",
    # DEFAULTCONNECTFLOODMSG in g_flood.h.
    "connectfloodprotectmsg": "Too many connections, try again later.",
    # "num sec cooldown" triples handled by an initfunc with no backing
    # scalar. Their Init functions leave the feature off unless all three
    # fields are supplied, so "disable" is the default state.
    "chatfloodprotect": "disable",
    "connectfloodprotect": "disable",
    "namechangefloodprotect": "disable",
    "skinchangefloodprotect": "disable",
    # Repeatable "<signal> <weight>" lines; there is no single default line.
    "signal_weight": "",
    # An empty list means every impulse counts (maxImpulses == 0).
    "impulsestokickon": "",
}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def parse_commands():
    """Every q2aCommands[] entry, as dicts."""
    src = strip_comments((SRC / "g_cmd.c").read_text())
    m = re.search(r"q2acmd_t q2aCommands\[\]\s*=\s*\{(.*?)\n\};", src, re.S)
    if not m:
        sys.exit("error: could not find the q2aCommands[] table in src/g_cmd.c")
    body = m.group(1)

    entries = []
    for blk in re.finditer(
        r'\{\s*("(?:[^"\\]|\\.)*")\s*,\s*([^,]+?)\s*,\s*(CMDTYPE_\w+)\s*,'
        r"\s*([^,}]*?)\s*(?:,\s*([^,}]*?)\s*)?(?:,\s*([^,}]*?)\s*)?,?\s*\}",
        body,
        re.S,
    ):
        entries.append(
            {
                "name": blk.group(1)[1:-1],
                "where": " ".join(blk.group(2).split()),
                "ctype": blk.group(3),
                "dp": (blk.group(4) or "").strip(),
            }
        )

    # Cross-check against a second, simpler count. A trailing comment or an
    # unusual entry layout can make the main pattern skip a row, and silently
    # dropping an option is exactly the failure this script exists to prevent.
    names = [n[1:-1] for n in re.findall(r'\{\s*\n\s*("(?:[^"\\]|\\.)*")\s*,', body)]
    missed = [n for n in names if n not in {e["name"] for e in entries}]
    if missed:
        sys.exit(f"error: failed to parse these q2aCommands[] entries: {missed}")
    return entries


def sources():
    return {p.name: strip_comments(p.read_text()) for p in SRC.glob("*.c")}


def struct_member_init(srcs, struct, member):
    """Read `.member = VALUE` out of `T struct = { ... };`."""
    for text in srcs.values():
        m = re.search(r"\b\w+\s+" + re.escape(struct) + r"\s*=\s*\{(.*?)\n\};", text, re.S)
        if not m:
            continue
        for d in re.finditer(r"\.(\w+)\s*=\s*([^,}]+)", m.group(1)):
            if d.group(1) == member:
                return d.group(2).strip()
        return None  # struct found but member left out, so zero/empty
    return "UNRESOLVED"


DECL = r"(bool|int|float|double|long|char|byte|short)"


def scalar_init(srcs, var):
    """Initializer text for a file-scope `var`, or None if it has none."""
    for text in srcs.values():
        m = re.search(
            r"^\s*(?:static\s+)?(?:unsigned\s+|signed\s+)?" + DECL + r"\s+"
            + re.escape(var) + r"\s*(?:\[[^\]]*\])?\s*=\s*([^;]+);",
            text,
            re.M,
        )
        if m:
            return m.group(2).strip()
        m = re.search(
            r"^\s*(?:static\s+)?(?:unsigned\s+|signed\s+)?" + DECL + r"\s+"
            + re.escape(var) + r"\s*(?:\[[^\]]*\])?\s*;",
            text,
            re.M,
        )
        if m:
            return None
    return "UNRESOLVED"


def array_element_init(srcs, base, member):
    """Handle `base[idx].member`, as the private/inverted command slots use.

    Only the zero-initialized case is resolvable this way: a file-scope array
    with no initializer leaves every member zero or empty. An array that IS
    initialized would need the right element picked out, so that errors out
    rather than guessing.
    """
    for text in srcs.values():
        m = re.search(
            r"^\s*(?:static\s+)?\w+\s+" + re.escape(base) + r"\s*\[[^\]]*\]\s*(=)?",
            text,
            re.M,
        )
        if m:
            return "UNRESOLVED" if m.group(1) else None
    return "UNRESOLVED"


def raw_default(srcs, entry):
    dp = entry["dp"]
    if dp in ("NULL", ""):
        return None
    inner = dp.lstrip("&").strip()
    if inner.startswith("(") and inner.endswith(")"):
        inner = inner[1:-1].strip()
    if "." in inner:
        struct, member = inner.split(".", 1)
        struct, member = struct.strip(), member.strip()
        if "[" in struct:
            return array_element_init(srcs, struct.split("[", 1)[0], member)
        return struct_member_init(srcs, struct, member)
    return scalar_init(srcs, inner)


def config_value(entry, raw):
    name, ctype = entry["name"], entry["ctype"]
    if name in SPECIAL:
        return SPECIAL[name]
    if raw == "UNRESOLVED":
        sys.exit(
            f"error: cannot find the variable behind '{name}' ({entry['dp']}).\n"
            f"       Add it to SPECIAL in {pathlib.Path(__file__).name} if it has no"
            f" single backing variable."
        )
    if ctype == "CMDTYPE_LOGICAL":
        return "Yes" if raw == "true" else "No"
    if ctype == "CMDTYPE_NUMBER":
        if raw is None:
            return "0"
        if re.fullmatch(r"-?\d+", raw):
            return raw
    if ctype == "CMDTYPE_STRING":
        if raw is None or raw == "{0}":
            return ""
        m = re.fullmatch(r'"(.*)"', raw, re.S)
        if m:
            return m.group(1)
    sys.exit(
        f"error: don't know how to express the default for '{name}'.\n"
        f"       type {ctype}, initializer {raw!r}.\n"
        f"       Add it to SPECIAL in {pathlib.Path(__file__).name}."
    )


def lint_configs(cfg_names):
    """Warn about settings the real parser would silently ignore.

    readCfgFile() matches with startContains(cmdname, token), ie the command
    name must START WITH the token from the file. A token no command name
    starts with matches nothing at all and the line does nothing.
    """
    problems = []
    for path in sorted(CFGDIR.glob("*.cfg")):
        if path.name == "example.cfg":
            continue
        for lineno, line in enumerate(path.read_text().splitlines(), 1):
            stripped = line.strip()
            if not stripped or stripped.startswith(";"):
                continue
            m = re.match(r"([A-Za-z0-9_]+)", stripped)
            if not m:
                continue
            token = m.group(1).lower()
            if not any(n.lower().startswith(token) for n in cfg_names):
                problems.append(f"{path.name}:{lineno}: '{m.group(1)}' matches no command")
    for p in problems:
        print(f"warning: {p}", file=sys.stderr)
    return problems


def main():
    # Resolved so it works whether make passes a relative path or it is run
    # by hand from anywhere.
    out = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_OUT
    srcs = sources()
    entries = parse_commands()
    cfg = [e for e in entries if "CMDCTX_CFGFILE" in e["where"]]
    if not cfg:
        sys.exit("error: no CMDCTX_CFGFILE commands found, the table parse is wrong")

    rows = sorted((e["name"], config_value(e, raw_default(srcs, e))) for e in cfg)

    width = max(len(name) for name, _ in rows) + 1
    out.write_text("".join(f'{name.ljust(width)}"{value}"\n' for name, value in rows))

    try:
        shown = out.relative_to(ROOT)
    except ValueError:
        shown = out
    print(f"{shown}: {len(rows)} options", file=sys.stderr)
    lint_configs([e["name"] for e in cfg])


if __name__ == "__main__":
    main()
