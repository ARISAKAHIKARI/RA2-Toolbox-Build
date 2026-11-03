#!/usr/bin/env python3
"""Embed the offline console view in a C++17 UTF-8 header."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def bundle(frontend: Path) -> str:
    html = (frontend / "index.html").read_text(encoding="utf-8")
    css = (frontend / "app.css").read_text(encoding="utf-8")
    script = (frontend / "app.js").read_text(encoding="utf-8")
    style_tag = '<link rel="stylesheet" href="app.css">'
    script_tag = '<script src="app.js"></script>'
    if html.count(style_tag) != 1 or html.count(script_tag) != 1:
        raise ValueError("console template must contain one app.css and app.js entry")
    css = css.replace("</style", "<\\/style")
    script = script.replace("</script", "<\\/script")
    html = html.replace(style_tag, '<style nonce="ra2-console">\n' + css + "\n</style>")
    html = html.replace(script_tag, '<script nonce="ra2-console">\n' + script + "\n</script>")
    if len(html.encode("utf-8")) > 1800000:
        raise ValueError("console HTML exceeds the NavigateToString content budget")
    return html


def header(html: str) -> str:
    delimiter = "RA2_CONSOLE_" + hashlib.sha256(html.encode("utf-8")).hexdigest()[:4]
    if ")" + delimiter + '"' in html:
        raise ValueError("raw-string delimiter collision")
    return (
        "#pragma once\n#include <cstddef>\nnamespace consoleassets {\n"
        f'inline constexpr char Html[] = u8R"{delimiter}({html}){delimiter}";\n'
        "inline constexpr std::size_t HtmlSize = sizeof(Html) - 1;\n}\n"
    )


def main() -> None:
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend", type=Path, default=source / "suite" / "frontend")
    parser.add_argument("--output", type=Path, help="optional self-contained HTML output")
    parser.add_argument("--header", type=Path, help="C++ header output")
    args = parser.parse_args()
    if args.output is None and args.header is None:
        args.output = source / "suite" / "frontend" / "console.html"
        args.header = source / "suite" / "source" / "web_console_assets.h"
    html = bundle(args.frontend)
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(html, encoding="utf-8")
    if args.header is not None:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(header(html), encoding="utf-8")
    print(f"Console view: {len(html.encode('utf-8'))} UTF-8 bytes")


if __name__ == "__main__":
    main()
