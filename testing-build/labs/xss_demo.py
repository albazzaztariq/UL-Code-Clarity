#!/usr/bin/env python3
"""
XSS (Cross-Site Scripting) Demo
---------------------------------
Simulates a comment board that inserts user input directly into HTML
using innerHTML — the browser executes any <script> tags.

Normal input  : Hello, great article!
Attack input  : <script>document.cookie</script>
"""
import sys
import re

def simulate_browser(html_content):
    """Simulate what a browser does when it sees innerHTML assignment."""
    # Detect script tags
    script_match = re.search(r'<script[^>]*>(.*?)</script>', html_content, re.IGNORECASE | re.DOTALL)
    img_match    = re.search(r'<img[^>]+onerror=["\']?([^"\'>\s]+)', html_content, re.IGNORECASE)

    if script_match:
        script_body = script_match.group(1).strip()
        return ("SCRIPT_EXECUTED", script_body)
    elif img_match:
        handler = img_match.group(1)
        return ("EVENT_HANDLER_EXECUTED", handler)
    else:
        return ("SAFE_TEXT", html_content)

def main():
    user_input = sys.stdin.read().strip()

    # VULNERABLE: direct innerHTML assignment — no sanitization
    page_html = f'<div id="comment">{user_input}</div>'

    print("=" * 60)
    print("BROWSER: Rendering comment with innerHTML...")
    print()
    print("  Generated HTML:")
    print("  " + page_html)
    print()

    result_type, result_data = simulate_browser(page_html)

    if result_type == "SCRIPT_EXECUTED":
        print("  [!] SCRIPT EXECUTED IN BROWSER!")
        print()
        if "cookie" in result_data.lower():
            print("  Script ran: " + result_data)
            print()
            print("  Stolen cookie: session=abc123def456")
            print("  Attacker now has your login session token.")
            print("  They can log into your account without your password.")
        else:
            print("  Script ran: " + result_data)
            print("  Arbitrary code executed in visitor's browser!")
    elif result_type == "EVENT_HANDLER_EXECUTED":
        print("  [!] EVENT HANDLER TRIGGERED!")
        print()
        print("  Handler: " + result_data)
        print("  Executed in every visitor's browser silently.")
    else:
        print("  Text rendered safely (no script detected):")
        print("  " + result_data)

    print("=" * 60)

if __name__ == "__main__":
    main()
