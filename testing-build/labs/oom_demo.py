#!/usr/bin/env python3
"""
Out-of-Memory (OOM) Demo
-------------------------
Allocates memory in a loop to simulate a memory exhaustion attack.
A safety cap prevents actually killing the host machine.

Normal mode  : allocate up to 50 MB (safe demo)
Attack mode  : show what unlimited allocation would do
"""
import sys
import os

SAFETY_CAP_MB = 200   # never exceed this in the demo
CHUNK_MB      = 10    # allocate this many MB per iteration

def format_mb(n_bytes):
    return f"{n_bytes / (1024*1024):.0f} MB"

def main():
    arg = sys.stdin.read().strip().lower()
    attack_mode = ("attack" in arg or "unlimited" in arg or "bomb" in arg)

    print("=" * 60)
    print("OOM DEMO: Allocating memory in a loop...")
    print()

    chunks = []
    total = 0

    try:
        for i in range(1, 1000):
            chunk = bytearray(CHUNK_MB * 1024 * 1024)   # allocate CHUNK_MB
            chunks.append(chunk)
            total += CHUNK_MB
            print(f"  Iteration {i:3d}: allocated {CHUNK_MB} MB  (total: {total} MB)")
            sys.stdout.flush()

            if not attack_mode and total >= 50:
                print()
                print("  [DEMO CAP REACHED at 50 MB]")
                print("  In a real attack, this loop runs forever.")
                break

            if total >= SAFETY_CAP_MB:
                print()
                print(f"  [SAFETY CAP at {SAFETY_CAP_MB} MB — stopping demo]")
                print("  A real OOM attack has no cap — the OS kills the process.")
                break

    except MemoryError:
        print()
        print(f"  [MemoryError] System ran out of memory after {total} MB!")
        print("  The OS killed the process.")

    print()
    if attack_mode:
        print("  ATTACK RESULT: Service crashed / killed by OS.")
        print("  All users lose access. This is a Denial of Service (DoS) attack.")
    else:
        print("  Normal demo complete. Memory freed when program exits.")
    print("=" * 60)

    # Release memory
    del chunks

if __name__ == "__main__":
    main()
