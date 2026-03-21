#!/usr/bin/env python3
"""
SQL Injection Demo
------------------
Simulates a login backend that builds a SQL query by string concatenation.
Reads username from stdin (first line) and password from stdin (second line).

Normal input  : alice / password123
Attack input  : ' OR '1'='1 / anything
"""
import sys

def simulate_db_query(query):
    """Pretend we ran the query and return simulated rows."""
    q = query.lower()
    # Detect tautology injection
    if "or '1'='1'" in q or 'or "1"="1"' in q or "or 1=1" in q:
        rows = [f"user_{i}" for i in range(1, 501)]
        return rows
    # Normal match — only return one row
    if "alice" in q:
        return ["alice"]
    if "admin" in q:
        return ["admin"]
    return []

def main():
    lines = sys.stdin.read().splitlines()
    username = lines[0] if len(lines) > 0 else ""
    password = lines[1] if len(lines) > 1 else ""

    # VULNERABLE: string concatenation builds the query
    query = "SELECT * FROM users WHERE username='" + username + "' AND password='" + password + "'"

    print("=" * 60)
    print("BACKEND: Building SQL query from user input...")
    print()
    print("  SQL Query:")
    print("  " + query)
    print()

    rows = simulate_db_query(query)

    if len(rows) > 1:
        print(f"  Query returned {len(rows)} rows — ALL users exposed!")
        print()
        print("  First 5 rows:")
        for r in rows[:5]:
            print(f"    - {r}")
        print(f"    ... and {len(rows)-5} more")
        print()
        print("  LOGIN SUCCESSFUL as: " + rows[0] + "  (attacker is now admin!)")
    elif len(rows) == 1:
        print(f"  Query returned 1 row: {rows[0]}")
        print()
        print("  LOGIN SUCCESSFUL as: " + rows[0])
    else:
        print("  Query returned 0 rows.")
        print()
        print("  LOGIN FAILED — invalid credentials.")

    print("=" * 60)

if __name__ == "__main__":
    main()
