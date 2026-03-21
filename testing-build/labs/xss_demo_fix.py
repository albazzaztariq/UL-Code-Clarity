#!/usr/bin/env python3
"""
XSS Demo — FIXED VERSION
--------------------------
Uses textContent (not innerHTML) — the browser treats the value as
plain text and never parses it as HTML or executes scripts.
"""
import sys
import html

def main():
    user_input = sys.stdin.read().strip()

    # FIXED: escape HTML entities before inserting into the DOM
    safe_content = html.escape(user_input)
    page_html = f'<div id="comment">{safe_content}</div>'

    print("=" * 60)
    print("BROWSER (FIXED): Rendering comment with textContent / escaped HTML...")
    print()
    print("  Raw input:     " + user_input)
    print("  Escaped input: " + safe_content)
    print()
    print("  Generated HTML:")
    print("  " + page_html)
    print()
    print("  The browser displays the text literally.")
    print("  '<script>' is shown as text, not executed as code.")
    print()
    print("  XSS attack neutralised.")
    print("=" * 60)

if __name__ == "__main__":
    main()
