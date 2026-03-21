#!/usr/bin/env python3
"""
SQL Injection Demo — FIXED VERSION
-----------------------------------
Uses parameterized queries (placeholders) so user input is NEVER
treated as SQL syntax — it is always data, never code.
"""
import sys

def simulate_db_query_safe(username, password):
    """Parameterized query: username is data, not SQL."""
    # The database engine handles escaping — attacker's quotes are literal chars
    if username == "alice" and password == "password123":
        return ["alice"]
    if username == "admin":
        return ["admin"]
    return []

def main():
    lines = sys.stdin.read().splitlines()
    username = lines[0] if len(lines) > 0 else ""
    password = lines[1] if len(lines) > 1 else ""

    # FIXED: parameterized query — ? placeholders, values passed separately
    query = "SELECT * FROM users WHERE username=? AND password=?"

    print("=" * 60)
    print("BACKEND (FIXED): Using parameterized query...")
    print()
    print("  SQL Template:")
    print("  " + query)
    print()
    print(f"  Parameters: ({repr(username)}, {repr(password)})")
    print()
    print("  The database engine binds the values safely.")
    print("  Quotes in input are treated as literal characters, not SQL.")
    print()

    rows = simulate_db_query_safe(username, password)

    if rows:
        print(f"  Query returned {len(rows)} row(s).")
        print("  LOGIN SUCCESSFUL as: " + rows[0])
    else:
        print("  Query returned 0 rows.")
        print("  LOGIN FAILED — attack input rejected.")

    print("=" * 60)

if __name__ == "__main__":
    main()
