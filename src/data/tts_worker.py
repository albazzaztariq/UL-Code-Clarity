#!/usr/bin/env python3
"""TTS worker for Code Clarity — reads sentences from stdin, speaks via edge-tts.
Runs as a long-lived subprocess. Send one sentence per line. Send "QUIT" to exit."""

import sys
import asyncio
import tempfile
import os

try:
    import edge_tts
except ImportError:
    print("ERROR: edge-tts not installed. pip install edge-tts", flush=True)
    sys.exit(1)

VOICE = os.environ.get("CC_TTS_VOICE", "en-US-GuyNeural")
AUDIO_PLAYER = "powershell"  # use PowerShell to play audio without blocking

async def speak(text):
    """Generate speech and play it."""
    if not text.strip():
        return
    try:
        with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as f:
            tmp = f.name

        communicate = edge_tts.Communicate(text, VOICE)
        await communicate.save(tmp)

        # Play audio using PowerShell (non-blocking-ish)
        proc = await asyncio.create_subprocess_exec(
            "powershell", "-NoProfile", "-Command",
            f"(New-Object Media.SoundPlayer '{tmp}').PlaySync(); Remove-Item '{tmp}'",
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.DEVNULL
        )
        await proc.wait()
    except Exception as e:
        print(f"TTS_ERROR: {e}", file=sys.stderr, flush=True)

async def main():
    print("TTS_READY", flush=True)
    loop = asyncio.get_event_loop()
    reader = asyncio.StreamReader()
    protocol = asyncio.StreamReaderProtocol(reader)
    await loop.connect_read_pipe(lambda: protocol, sys.stdin)

    while True:
        line = await reader.readline()
        if not line:
            break
        text = line.decode("utf-8", errors="replace").strip()
        if text == "QUIT":
            break
        if text:
            await speak(text)

if __name__ == "__main__":
    asyncio.run(main())
