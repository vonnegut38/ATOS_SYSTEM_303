#!/usr/bin/env python3
"""Extract shipped locale dictionaries (en/zh) from DSH client plugin bundles.

The built `lib/client.js` of every client plugin registers its dictionaries in
one of two shapes:

    ctx.locale.register("job", { zh, en })
    ctx.locale.register(NS, { zh, en })          // const NS = "…"
    ctx.locale.register(LOCALE_NS, locale, dict) // per-locale language pack

with the dictionary literals declared in the same bundle as `const en = {...}`.
Both are plain data, so a brace-matching parser that respects string literals
and comments is enough to recover them without a JS engine.

Output: one JSON file per namespace under <out>/<namespace>.json holding
{"source": <package>, "en": {...}, "zh": {...}}.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

CALL_RE = re.compile(r"locale\.register\(")


def skip_ws(src: str, i: int) -> int:
    while i < len(src) and src[i] in " \t\r\n":
        i += 1
    return i


def read_literal(src: str, i: int) -> tuple[str, int]:
    """Read a JS string literal starting at i; return (value, end_index)."""
    quote = src[i]
    assert quote in "\"'"
    i += 1
    out: list[str] = []
    while i < len(src):
        c = src[i]
        if c == "\\":
            nxt = src[i + 1]
            out.append({"n": "\n", "t": "\t", "r": "\r"}.get(nxt, nxt))
            i += 2
            continue
        if c == quote:
            return "".join(out), i + 1
        out.append(c)
        i += 1
    raise ValueError("unterminated string literal")


def match_balanced(src: str, start: int) -> int:
    """Given index of an opening bracket, return index just past its match."""
    opening = src[start]
    closing = {"{": "}", "(": ")", "[": "]"}[opening]
    depth = 0
    i = start
    n = len(src)
    while i < n:
        c = src[i]
        if c in "\"'`":
            quote = c
            i += 1
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == quote:
                    break
                i += 1
            i += 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            end = src.find("*/", i + 2)
            i = n if end < 0 else end + 2
            continue
        if c == opening:
            depth += 1
        elif c == closing:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError(f"unbalanced {opening!r}")


def split_top_level(text: str) -> list[str]:
    """Split an argument list on top-level commas."""
    parts: list[str] = []
    depth = 0
    start = 0
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c in "\"'`":
            quote = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    break
                i += 1
            i += 1
            continue
        if c in "{[(":
            depth += 1
        elif c in "}])":
            depth -= 1
        elif c == "," and depth == 0:
            parts.append(text[start:i])
            start = i + 1
        i += 1
    parts.append(text[start:])
    return [p.strip() for p in parts if p.strip()]


def find_const(src: str, name: str, opener: str) -> str | None:
    """Find `<const|let|var> <name> = <opener>…` and return the literal text."""
    pattern = re.compile(
        r"\b(?:const|let|var)\s+" + re.escape(name) + r"\s*=\s*" + re.escape(opener)
    )
    m = pattern.search(src)
    if not m:
        return None
    start = m.end() - 1
    if opener in "\"'":
        value, end = read_literal(src, start)
        return src[start:end]
    return src[start : match_balanced(src, start)]


def resolve_namespace(src: str, expr: str) -> str | None:
    expr = expr.strip()
    if expr.startswith(('"', "'")):
        return read_literal(expr, 0)[0]
    literal = find_const(src, expr, '"') or find_const(src, expr, "'")
    if literal is not None:
        try:
            return read_literal(literal, 0)[0]
        except ValueError:
            return None
    # Concatenated constants, e.g. const NS = "settings." + "general"
    literal = find_const(src, expr, '"')
    return None


def parse_flat_object(literal: str) -> dict[str, str]:
    """Parse a flat object literal of string keys to string values."""
    body = literal[1:-1]
    out: dict[str, str] = {}
    for part in split_top_level(body):
        m = re.match(r'^\s*(?:"([^"]*)"|\'([^\']*)\'|([A-Za-z_$][\w$]*))\s*:\s*(.+)$', part, re.S)
        if not m:
            continue
        key = m.group(1) or m.group(2) or m.group(3)
        value = m.group(4).strip()
        if value.startswith(('"', "'")):
            try:
                out[key] = read_literal(value, 0)[0]
            except ValueError:
                continue
        else:
            out[key] = value  # identifier — resolved by the caller
    return out


def identifier_map(literal: str) -> dict[str, str]:
    """Parse `{ zh, en }` or `{ zh: zh$1, en: en$1 }` into key -> identifier."""
    body = literal[1:-1]
    out: dict[str, str] = {}
    for part in split_top_level(body):
        m = re.match(r'^\s*(?:"([^"]*)"|\'([^\']*)\'|([A-Za-z_$][\w$]*))\s*(?::\s*(.+))?$', part, re.S)
        if not m:
            continue
        key = m.group(1) or m.group(2) or m.group(3)
        value = (m.group(4) or key).strip()
        out[key] = value
    return out


def take_dictionary(src: str, expr: str) -> dict[str, str] | None:
    """Resolve an expression that denotes a dictionary object."""
    expr = expr.strip()
    if expr.startswith("{"):
        parsed = parse_flat_object(expr)
        if parsed and all(not re.fullmatch(r"[A-Za-z_$][\w$]*", v) for v in parsed.values()):
            return parsed
        return None
    literal = find_const(src, expr, "{")
    if literal is None:
        return None
    return parse_flat_object(literal)


MEMBER_RE = re.compile(
    r"^([A-Za-z_$][\w$]*)\[\s*(?:\"((?:[^\"\\]|\\.)*)\"|'((?:[^'\\]|\\.)*)')\s*\]$"
)


def resolve_identifier_values(src: str, dictionary: dict[str, str]) -> dict[str, str]:
    """Replace identifier-shaped values with the string literal they name.

    A dictionary entry written as property shorthand over a module constant
    (`"hint.plan": PLAN_NEXT_ACTION_EN`) parses as an identifier, and one written
    as an indexed read of a per-locale table (`"confirm.title":
    accessEn["confirm.title"]`) parses as a member expression. Left alone either
    would enter the catalog as if it were the English copy, so both shapes are
    resolved against constants declared in the same bundle.
    """
    resolved: dict[str, str] = {}
    for key, value in dictionary.items():
        if re.fullmatch(r"[A-Za-z_$][\w$]*", value) and value not in ("true", "false", "null"):
            literal = find_const(src, value, '"') or find_const(src, value, "'")
            if literal is not None:
                try:
                    resolved[key] = read_literal(literal, 0)[0]
                    continue
                except ValueError:
                    pass
        member = MEMBER_RE.match(value)
        if member is not None:
            table = find_const(src, member.group(1), "{")
            if table is not None:
                entries = parse_flat_object(table)
                lookup = member.group(2) if member.group(2) is not None else member.group(3)
                if lookup in entries:
                    resolved[key] = entries[lookup]
                    continue
        resolved[key] = value
    return resolved


def scan_bundle(package: str, src: str) -> tuple[dict[str, dict[str, dict]], list[str]]:
    found: dict[str, dict[str, dict]] = {}
    warnings: list[str] = []
    for m in CALL_RE.finditer(src):
        paren = m.end() - 1
        try:
            args_end = match_balanced(src, paren)
        except ValueError:
            continue
        args = split_top_level(src[paren + 1 : args_end - 1])
        if len(args) < 2:
            continue
        namespace = resolve_namespace(src, args[0])
        if not namespace:
            warnings.append(f"{package}: unresolved namespace {args[0][:40]!r}")
            continue
        entry = found.setdefault(namespace, {})
        if len(args) == 2 and args[1].startswith("{"):
            mapping = identifier_map(args[1])
            for lang in ("en", "zh"):
                ident = mapping.get(lang)
                if not ident:
                    continue
                if ident == lang:
                    literal = find_const(src, ident, "{")
                    entry[lang] = (
                        resolve_identifier_values(src, parse_flat_object(literal)) if literal else {}
                    )
                else:
                    dict_ = take_dictionary(src, ident)
                    if dict_ is not None:
                        entry[lang] = resolve_identifier_values(src, dict_)
        elif len(args) == 3:
            lang = args[1].strip().strip("\"'")
            dict_ = take_dictionary(src, args[2])
            if dict_ is not None:
                entry[lang] = resolve_identifier_values(src, dict_)
    return found, warnings


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--packages",
        default="/Users/xxx/.npm/_npx/1e7f6d9597241db0/node_modules/@deepseek-ai",
        help="directory holding @deepseek-ai packages",
    )
    parser.add_argument("--out", required=True, help="output directory")
    args = parser.parse_args()

    catalog: dict[str, dict] = {}
    warnings: list[str] = []
    for bundle in sorted(Path(args.packages).glob("*/lib/client.js")):
        package = bundle.parent.parent.name
        try:
            src = bundle.read_text(encoding="utf8")
        except UnicodeDecodeError:
            warnings.append(f"{package}: not utf8")
            continue
        found, warn = scan_bundle(package, src)
        warnings.extend(warn)
        for namespace, langs in found.items():
            en = langs.get("en") or {}
            if not en:
                warnings.append(f"{package}: namespace {namespace!r} has no en keys")
                continue
            if namespace in catalog and catalog[namespace]["source"] != package:
                warnings.append(
                    f"{namespace!r}: {catalog[namespace]['source']} and {package} both register it"
                )
                continue
            catalog[namespace] = {
                "source": package,
                "en": en,
                "zh": langs.get("zh") or {},
            }

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for namespace, data in catalog.items():
        (out / f"{namespace}.json").write_text(
            json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf8",
        )

    total_keys = sum(len(d["en"]) for d in catalog.values())
    print(f"namespaces: {len(catalog)}  keys: {total_keys}\n")
    print(f"{'namespace':40} {'keys':>5}  source")
    for namespace in sorted(catalog, key=lambda n: -len(catalog[n]["en"])):
        data = catalog[namespace]
        print(f"{namespace:40} {len(data['en']):5d}  {data['source']}")
    if warnings:
        print("\nwarnings:")
        for warning in sorted(set(warnings)):
            print(" -", warning)


if __name__ == "__main__":
    main()
