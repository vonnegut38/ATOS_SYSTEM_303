#!/usr/bin/env python3
"""Generate `lib/client.js` (the browser bundle) from `translations/*.json`.

The DSH client bundle format is a single `window.__ModuleLoader__.load({...})`
call whose factory returns the plugin exports. The pack needs no imports, so
the bundle is generated directly instead of being run through a bundler.

The generator is deliberately strict: a translation whose key set does not
match the shipped English dictionary of that namespace is rejected, so a typo
in a key can never silently produce a key that never renders.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

LANGUAGE = {"id": "pl", "label": "Polski", "fallback": "en"}
PACKAGE_NAME = "dsh-locale-pl"

HEADER = """/**
 * Polish language pack for the DSH web GUI — generated file, do not edit.
 *
 * Source of truth: `translations/<namespace>.json`, built by
 * `tools/build_bundle.py`. Regenerate with:
 *
 *     python3 tools/build_bundle.py
 */
window.__ModuleLoader__.load({{
\tid: {package!r},
\tfactory: (require) => {{
\t\tvar module = {{ exports: {{}} }};
\t\tvar exports = module.exports;
\t\tObject.defineProperty(exports, Symbol.toStringTag, {{ value: "Module" }});
\t\t/** Language definition appended to the built-in en/zh catalog. */
\t\tconst LANGUAGE = {language};
\t\t/**
\t\t * Per-namespace Polish dictionaries. Every namespace falls back to the
\t\t * shipped English dictionary for keys this pack does not translate.
\t\t */
\t\tconst DICTIONARIES = {{
"""

FOOTER = """\t\t};
\t\t/** Client services this plugin needs before it can register. */
\t\tconst inject = ["locale"];
\t\t/** Register the language, then each namespace dictionary. */
\t\tfunction apply(ctx) {
\t\t\tctx.effect(() => ctx.locale.addLanguage(LANGUAGE), "locale-pl: language");
\t\t\tfor (const [ns, dict] of Object.entries(DICTIONARIES)) {
\t\t\t\tctx.effect(() => ctx.locale.register(ns, LANGUAGE.id, dict), `locale-pl: ${ns} dictionary`);
\t\t\t}
\t\t}
\t\texports.apply = apply;
\t\texports.inject = inject;
\t\treturn module.exports;
\t}
});
"""


def escape_for_script(text: str) -> str:
    """Escape `<` so no translated string can close a surrounding script tag."""
    return json.dumps(text, ensure_ascii=False).replace("<", "\\u003c")


def load_namespaces(translations_dir: Path, catalog_dir: Path) -> dict[str, dict[str, str]]:
    namespaces: dict[str, dict[str, str]] = {}
    for path in sorted(translations_dir.glob("*.json")):
        namespace = path.stem
        catalog_path = catalog_dir / f"{namespace}.json"
        if not catalog_path.exists():
            raise SystemExit(f"{path}: no matching catalog/{namespace}.json")
        source = json.loads(catalog_path.read_text(encoding="utf8"))["en"]
        translated = json.loads(path.read_text(encoding="utf8"))
        extra = sorted(set(translated) - set(source))
        if extra:
            raise SystemExit(f"{namespace}: keys absent from the shipped dictionary: {extra[:10]}")
        empty = [key for key, value in translated.items() if not str(value).strip()]
        if empty:
            raise SystemExit(f"{namespace}: empty values: {empty[:10]}")
        namespaces[namespace] = translated
    if not namespaces:
        raise SystemExit("no translations found")
    return namespaces


def render(namespaces: dict[str, dict[str, str]]) -> str:
    parts = [HEADER.format(package=PACKAGE_NAME, language=escape_for_script(LANGUAGE))]
    for namespace, dictionary in namespaces.items():
        parts.append(f"\t\t\t{escape_for_script(namespace)}: {{\n")
        for key, value in dictionary.items():
            parts.append(f"\t\t\t\t{escape_for_script(key)}: {escape_for_script(value)},\n")
        parts.append("\t\t\t},\n")
    parts.append(FOOTER)
    return "".join(parts)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--check", action="store_true", help="fail when lib/client.js is stale")
    args = parser.parse_args()

    root = Path(args.root)
    namespaces = load_namespaces(root / "translations", root / "catalog")
    output = render(namespaces)
    target = root / "lib" / "client.js"

    if args.check:
        current = target.read_text(encoding="utf8") if target.exists() else ""
        if current != output:
            raise SystemExit("lib/client.js is stale — run tools/build_bundle.py")
        print(f"lib/client.js up to date ({len(namespaces)} namespaces)")
        return

    target.write_text(output, encoding="utf8")
    keys = sum(len(d) for d in namespaces.values())
    print(f"wrote {target.relative_to(root)}: {len(namespaces)} namespaces, {keys} keys, {len(output)} bytes")


if __name__ == "__main__":
    main()
