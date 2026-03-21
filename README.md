# Code Clarity

**A code editor and learning platform for those new to programming or anyone looking to tie explicit learning to their coding work**

This tool makes learning an explicit facet of all your work. You build with AI, and every change is explained — what happened, why, and what you'd need to know to do it yourself. Whether you're writing your first line of code or you've been vibe-coding with AI tools and want to actually understand what's being generated — this is where you go from "it works but I don't know why" to "I built this and I understand every line."

---

## Screenshot

*Screenshot coming soon.*

---

## Key Features

### Clarity — The Learning Engine
Every AI-generated change produces a plain-English entry in the Clarity panel: what changed, why, and what concepts were used. Click any entry to highlight the relevant code, see a structured explanation, and optionally try writing it yourself. No other tool does this.

### AI Chat
Describe what you want in plain English. The model generates code and inserts it directly into the editor. Bring your own API key — supports Anthropic, OpenAI, Google, and local models via Ollama.

### Highlight and Explain
Select any code. Right-click → Explain This. Get a plain-English paragraph explaining exactly what that code does, step by step.

### Runtime Toolpane
A visible panel showing what the runtime looks like for the active language — memory model, threading, compilation, and more. For C, Python, and Rust, you see what happens. For UniLogic, you control it: switch memory models, concurrency strategies, and build targets live.

### Statement Expander
One-click expansion of nested and chained expressions into clearly readable, one-operation-per-line code. Each expanded block is marked; the original is preserved.

### Codebase Explainer
Two modes: Structure (what files exist and how they relate) and Execution (what happens when the program runs — call flow, entry point, data paths).

### Language Guide and Built-in Tutorials
Progressive examples per language, from Hello World to error handling. Some examples intentionally contain errors for active learning. Callstack visualization in tutorial mode.

### Build Panel
Run or build with one click. No terminal required. Results appear in the results pane. UL projects support WASM and VM targets alongside native.

### Memory Safety Check (C/C++)
On-demand scan for malloc without free, use-after-free, buffer overflows, uninitialized variables, and double free. Presented as plain-English suggestions, not warnings.

### Progressive Disclosure
Start with a single file and a Run button. Unlock multi-file tabs, a terminal, git basics, and full IDE power as you need them — or jump straight to any level.

---

## Supported Languages

- C
- C++
- Python
- Rust
- UniLogic (UL)

---

## Platforms

| Platform | Status |
|----------|--------|
| Windows x64 | Available |
| macOS ARM64 | Coming soon |

---

## Download and Install

*Release builds coming in Phase 2. Watch this repo for updates.*

---

## License

MIT
