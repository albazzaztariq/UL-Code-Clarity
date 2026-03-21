#include "core/machineview.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QRegularExpression>
#include <QApplication>

// ============================================================================
// MachineViewPanel — constructor
// ============================================================================

MachineViewPanel::MachineViewPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(180);
    setStyleSheet("background: #181825; border-top: 1px solid #313244;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(32);
    header->setStyleSheet("background: #181825; border-bottom: 1px solid #313244;");
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(12, 0, 8, 0);
    hLayout->setSpacing(8);

    m_titleLabel = new QLabel("Machine View");
    m_titleLabel->setStyleSheet("color: #cdd6f4; font-size: 12px; font-weight: bold;"
                                " background: transparent;");
    hLayout->addWidget(m_titleLabel);

    m_lineLabel = new QLabel;
    m_lineLabel->setStyleSheet("color: #585b70; font-size: 11px; background: transparent;");
    hLayout->addWidget(m_lineLabel);

    hLayout->addStretch();

    auto* hintLabel = new QLabel("Click any line in the editor");
    hintLabel->setStyleSheet("color: #45475a; font-size: 11px; background: transparent;");
    hLayout->addWidget(hintLabel);

    m_closeBtn = new QPushButton("x");
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: none; color: #585b70; font-size: 13px; border: none; }"
        "QPushButton:hover { color: #f38ba8; }");
    hLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(header);

    // ── Content area ──────────────────────────────────────────────────────────
    auto* content = new QWidget;
    content->setStyleSheet("background: #1e1e2e;");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 10, 16, 10);
    contentLayout->setSpacing(6);

    // Empty state
    m_emptyLabel = new QLabel("Select a line above to see what the computer does.");
    m_emptyLabel->setStyleSheet("color: #45475a; font-size: 12px; background: transparent;");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_emptyLabel);

    // Headline
    m_headLabel = new QLabel;
    m_headLabel->setStyleSheet("color: #89b4fa; font-size: 13px; font-weight: bold;"
                               " background: transparent;");
    m_headLabel->setWordWrap(true);
    m_headLabel->hide();
    contentLayout->addWidget(m_headLabel);

    // Body
    m_bodyLabel = new QLabel;
    m_bodyLabel->setStyleSheet("color: #cdd6f4; font-size: 12px; background: transparent;"
                               " line-height: 1.4;");
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->hide();
    contentLayout->addWidget(m_bodyLabel);

    contentLayout->addStretch();
    mainLayout->addWidget(content, 1);

    connect(m_closeBtn, &QPushButton::clicked, this, &MachineViewPanel::closeRequested);
}

void MachineViewPanel::setLevel(int level)
{
    m_level = qBound(1, level, 4);
}

void MachineViewPanel::setLanguage(const QString& lang)
{
    m_lang = lang.toLower();
}

void MachineViewPanel::applyTheme(bool isDark)
{
    m_isDark = isDark;
    // Theme is mostly hardcoded dark; could update here
}

void MachineViewPanel::clearExplanation()
{
    m_lineLabel->clear();
    m_headLabel->hide();
    m_bodyLabel->hide();
    m_emptyLabel->show();
}

void MachineViewPanel::explainLine(int lineNumber, const QString& lineText)
{
    QString trimmed = lineText.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith("//") || trimmed.startsWith('#')
        || trimmed.startsWith("/*") || trimmed == "{" || trimmed == "}") {
        clearExplanation();
        m_lineLabel->setText(QString("Line %1 — no operation").arg(lineNumber));
        return;
    }

    m_lineLabel->setText(QString("Line %1").arg(lineNumber));

    MachineExplanation expl = generateExplanation(trimmed, m_lang, m_level);

    if (expl.isEmpty()) {
        m_headLabel->hide();
        m_bodyLabel->hide();
        m_emptyLabel->setText(
            QString("Line %1: \"%2\"\n\nNo detailed explanation available for this pattern.")
                .arg(lineNumber)
                .arg(trimmed.left(60)));
        m_emptyLabel->show();
        return;
    }

    m_emptyLabel->hide();
    m_headLabel->setText(expl.headline);
    m_headLabel->show();
    m_bodyLabel->setText(expl.body);
    m_bodyLabel->show();
}

// ============================================================================
// Pattern dispatch
// ============================================================================

MachineExplanation MachineViewPanel::generateExplanation(const QString& line,
                                                          const QString& lang,
                                                          int level) const
{
    MachineExplanation e;

    // Try each pattern in order of specificity
    if (!(e = tryImport(line, lang, level)).isEmpty())         return e;
    if (!(e = tryPrint(line, lang, level)).isEmpty())          return e;
    if (!(e = tryReturn(line, lang, level)).isEmpty())         return e;
    if (!(e = tryIfElse(line, lang, level)).isEmpty())         return e;
    if (!(e = tryLoop(line, lang, level)).isEmpty())           return e;
    if (!(e = tryCMalloc(line, lang, level)).isEmpty())        return e;
    if (!(e = tryCFree(line, lang, level)).isEmpty())          return e;
    if (!(e = tryCPointer(line, lang, level)).isEmpty())       return e;
    if (!(e = tryCDeclaration(line, lang, level)).isEmpty())   return e;
    if (!(e = tryCArray(line, lang, level)).isEmpty())         return e;
    if (!(e = tryStringConcat(line, lang, level)).isEmpty())   return e;
    if (!(e = tryListOp(line, lang, level)).isEmpty())         return e;
    if (!(e = tryArithmetic(line, lang, level)).isEmpty())     return e;
    if (!(e = tryFunctionCall(line, lang, level)).isEmpty())   return e;
    if (!(e = tryVariableAssignment(line, lang, level)).isEmpty()) return e;

    return {};
}

// ============================================================================
// Pattern: import
// ============================================================================
MachineExplanation MachineViewPanel::tryImport(const QString& line, const QString& lang, int level) const
{
    if (!line.startsWith("import ") && !line.startsWith("from ") &&
        !line.startsWith("#include")) return {};
    if (lang == "python") {
        switch (level) {
        case 1: return { "Loading a module",
            "Python finds the module file, runs it (if not cached), and makes its contents available under the import name." };
        case 2: return { "Module import",
            "Python checks sys.modules first. If not cached, it finds the .py file on sys.path, compiles it to bytecode (.pyc), executes the module body in a new namespace, and caches it. The name is bound in the current namespace." };
        case 3: return { "PyImport_ImportModule",
            "Calls PyImport_ImportModule → _find_and_load → _find_spec → SourceFileLoader.exec_module. Bytecode is compiled by compile() and executed via PyEval_EvalCode. Module added to sys.modules dict." };
        default: return { "Import at address level",
            "dlopen() / LoadLibrary equivalent for C extensions. For pure Python: read .py file → tokenize → parse → compile to code object → marshal to .pyc → PyEval_EvalCode in module dict." };
        }
    } else {
        switch (level) {
        case 1: return { "Including a header",
            "The compiler copies the contents of that header file here, making its declarations available." };
        case 2: return { "#include preprocessing",
            "The C preprocessor (cpp) opens the header file, reads its text, and inserts it at this point in the translation unit before compilation begins." };
        case 3: return { "Preprocessor directive",
            "cpp opens the file at the specified include path, recursively processes its directives, and inlines the resulting token stream into the current translation unit. Guard macros (#ifndef) prevent re-inclusion." };
        default: return { "Header inclusion",
            "File read → token stream merged → symbol table entries added for each declaration. No machine instructions generated; purely a compile-time operation." };
        }
    }
}

// ============================================================================
// Pattern: print / printf
// ============================================================================
MachineExplanation MachineViewPanel::tryPrint(const QString& line, const QString& lang, int level) const
{
    bool isPrint   = line.startsWith("print(");
    bool isPrintf  = line.startsWith("printf(") || line.startsWith("puts(")
                     || line.startsWith("fprintf(");
    if (!isPrint && !isPrintf) return {};

    if (lang == "python") {
        switch (level) {
        case 1: return { "Printing to the screen",
            "Python converts whatever is inside print() to text and sends it to the terminal." };
        case 2: return { "sys.stdout.write",
            "Python converts each argument to a string using str(), joins them with sep (default space), appends end (default newline), and writes the result to sys.stdout. sys.stdout is usually a TextIOWrapper around file descriptor 1." };
        case 3: return { "PyObject_CallMethod → write",
            "print() calls sys.stdout.write(text + '\\n'). TextIOWrapper.write() encodes the string (e.g., UTF-8), then calls the underlying FileIO.write, which calls the write(1, buf, len) syscall." };
        default: return { "write(1, buf, n) syscall",
            "CALL write syscall: rax=1 (SYS_write), rdi=1 (stdout fd), rsi=ptr to UTF-8 buffer, rdx=byte count. Kernel copies buffer to terminal driver ring buffer." };
        }
    } else {
        switch (level) {
        case 1: return { "Printing to the screen",
            "The program sends formatted text to the terminal output." };
        case 2: return { "printf formatting",
            "printf parses the format string, formats each argument according to its specifier (%d, %s, etc.), and writes the result to stdout (file descriptor 1) via the C library." };
        case 3: return { "vfprintf → fwrite → write(1,...)",
            "printf → vfprintf → __vfprintf_internal → buffer the formatted bytes in stdout's buffer. fflush or newline flushes via write(STDOUT_FILENO, buf, n)." };
        default: return { "write syscall path",
            "push args → call printf@PLT → format to internal buffer → call write(1, buf, len) → INT 0x80 / SYSCALL instruction → kernel writes to terminal." };
        }
    }
}

// ============================================================================
// Pattern: return
// ============================================================================
MachineExplanation MachineViewPanel::tryReturn(const QString& line, const QString& lang, int level) const
{
    if (!line.startsWith("return")) return {};
    QString val = line.mid(6).trimmed();

    if (lang == "python") {
        switch (level) {
        case 1: return { "Giving back a value",
            QString("The function sends back '%1' to wherever it was called from, then stops running.").arg(val.isEmpty() ? "None" : val) };
        case 2: return { "Function return",
            "Python places the return value into the TOS (top of stack) slot, restores the previous frame's local variable table, and resumes execution at the call site." };
        case 3: return { "RETURN_VALUE opcode",
            "Bytecode: RETURN_VALUE. The eval loop pops TOS, stores it in the caller's frame->f_stacktop[0], decrements f_code->co_stacksize, pops the frame from the call stack, and DECREF's the old frame." };
        default: return { "Stack frame teardown",
            "mov rax, [return_value_ptr]  ; load return value\npop rbp                      ; restore caller's frame\nret                          ; jump to return address" };
        }
    } else {
        switch (level) {
        case 1: return { "Giving back a value",
            QString("The function finishes and sends back '%1' to the caller.").arg(val.isEmpty() ? "nothing" : val) };
        case 2: return { "Function return",
            "The return value is placed in the return register (usually RAX for integers). The function cleans up its stack frame, restores saved registers, and jumps back to the caller." };
        case 3: return { "Stack frame epilogue",
            "Value moved to RAX (or XMM0 for float). Stack pointer restored: add rsp, frame_size. Callee-saved registers restored from stack. Return address popped from stack into RIP." };
        default: return { "ret instruction",
            QString("mov eax, %1  ; return value in eax\nadd rsp, N   ; deallocate locals\npop rbp      ; restore base pointer\nret          ; pop rip from stack, jump to caller").arg(val.isEmpty() ? "0" : val) };
        }
    }
}

// ============================================================================
// Pattern: if/elif/else
// ============================================================================
MachineExplanation MachineViewPanel::tryIfElse(const QString& line, const QString& lang, int level) const
{
    bool isIf   = line.startsWith("if ") || line.startsWith("if(");
    bool isElif = line.startsWith("elif ");
    bool isElse = line == "else:" || line == "else" || line == "} else {" || line.startsWith("else {");
    if (!isIf && !isElif && !isElse) return {};

    if (lang == "python") {
        switch (level) {
        case 1: return { "Making a decision",
            "Python checks the condition. If it's true, the indented block runs. Otherwise Python skips it (or runs the else block)." };
        case 2: return { "Branch evaluation",
            "Python evaluates the condition expression to a Python object, then calls bool() on it. If truthy, execution continues into the if block; otherwise the interpreter jumps to the elif/else branch or the next statement." };
        case 3: return { "POP_JUMP_IF_FALSE opcode",
            "Bytecode: evaluate condition → LOAD result → POP_JUMP_IF_FALSE <else_label>. Python calls PyObject_IsTrue(ob) which checks ob's tp_as_number->nb_bool or tp_as_mapping->mp_length." };
        default: return { "Conditional jump",
            "cmp rax, 0      ; test condition\nje  else_label   ; jump if zero (false)\n; ... if block ...\njmp end_label\nelse_label:" };
        }
    } else {
        switch (level) {
        case 1: return { "Making a decision",
            "The computer checks the condition. If true, the block inside runs; otherwise it's skipped." };
        case 2: return { "Conditional branch",
            "The condition is evaluated (arithmetic, pointer comparison, etc.). The result sets flags in the processor's FLAGS register. A conditional jump instruction (JE, JNE, JL, etc.) checks those flags and jumps to the else branch or falls through." };
        case 3: return { "FLAGS register + Jcc instruction",
            "CMP/TEST sets ZF, SF, OF, CF flags. Jcc instruction reads flags and either increments RIP normally (branch not taken) or loads the target address into RIP (branch taken). Branch predictor caches the likely path." };
        default: return { "Conditional jump x86",
            "cmp operand1, operand2\njne  else_branch   ; or je/jl/jg depending on condition\n; true branch code\njmp  end_if\nelse_branch:\n; false branch code\nend_if:" };
        }
    }
}

// ============================================================================
// Pattern: for/while loop
// ============================================================================
MachineExplanation MachineViewPanel::tryLoop(const QString& line, const QString& lang, int level) const
{
    bool isFor   = line.startsWith("for ") || line.startsWith("for(");
    bool isWhile = line.startsWith("while ") || line.startsWith("while(");
    if (!isFor && !isWhile) return {};

    if (lang == "python") {
        if (isFor) {
            switch (level) {
            case 1: return { "Repeating for each item",
                "Python goes through each item in the collection one at a time, running the indented block for each one." };
            case 2: return { "Iterator protocol",
                "Python calls iter() on the iterable to get an iterator object, then repeatedly calls next() on it. Each call to next() returns the next item or raises StopIteration to end the loop." };
            case 3: return { "GET_ITER / FOR_ITER opcodes",
                "GET_ITER: calls ob->ob_type->tp_iter(ob). FOR_ITER: calls iternextfunc (ob->ob_type->tp_iternext). If NULL returned and StopIteration raised, jump to after-loop target." };
            default: return { "Iterator at address level",
                "CALL tp_iter → PyListIter object allocated (heap). Loop: CALL tp_iternext → pointer arithmetic on array + index. Each return: INCREF new item, DECREF old item." };
            }
        } else {
            switch (level) {
            case 1: return { "Repeating while true",
                "Python keeps running the indented block over and over as long as the condition remains true." };
            case 2: return { "Condition re-evaluated each iteration",
                "At the top of each loop iteration, Python re-evaluates the while condition. If it becomes false, the interpreter jumps past the loop body." };
            case 3: return { "JUMP_BACKWARD opcode",
                "At loop end: JUMP_BACKWARD to the condition test. Condition evaluates → POP_JUMP_IF_FALSE to after-loop. Hot loops may be compiled by the adaptive specializing interpreter." };
            default: return { "Loop as conditional jump",
                "loop_start:\n  ; evaluate condition\n  cmp rax, 0\n  je  loop_end\n  ; body\n  jmp loop_start\nloop_end:" };
            }
        }
    } else {
        if (isFor) {
            switch (level) {
            case 1: return { "Counting loop",
                "The computer repeats the block, counting the variable through its range." };
            case 2: return { "For loop structure",
                "The initializer runs once. Before each iteration the condition is checked; if false the loop ends. The increment runs after each iteration body." };
            case 3: return { "Counter register + conditional jump",
                "Initializer sets counter register. Test via CMP + Jcc at top (or bottom) of loop. Increment: ADD/INC instruction. Modern compilers may unroll or vectorize hot loops." };
            default: return { "Loop in assembly",
                "mov ecx, N      ; counter\nloop_start:\n  ; body\n  dec ecx\n  jnz loop_start  ; jump if ecx != 0" };
            }
        } else {
            switch (level) {
            case 1: return { "While loop",
                "The computer checks the condition, runs the block, then checks again — repeating until the condition is false." };
            case 2: return { "Condition at top of loop",
                "Each iteration begins with a condition evaluation. If false, a jump instruction transfers control past the loop body." };
            case 3: return { "Conditional backward jump",
                "while_top:\n  CMP operands → flags\n  Jcc while_end\n  ; body\n  JMP while_top\nwhile_end:" };
            default: return { "While as jump sequence",
                "while_top:\n  cmp condition\n  je while_end\n  ; body instructions\n  jmp while_top\nwhile_end:" };
            }
        }
    }
}

// ============================================================================
// Pattern: string concatenation
// ============================================================================
MachineExplanation MachineViewPanel::tryStringConcat(const QString& line, const QString& lang, int level) const
{
    // Detect: var = ... + ... with at least one string hint
    if (!line.contains('+') && !line.contains("join(")) return {};
    if (lang == "python") {
        bool hasStr = line.contains('"') || line.contains('\'') || line.contains("str(");
        bool hasJoin= line.contains(".join(");
        if (!hasStr && !hasJoin) return {};

        switch (level) {
        case 1: return { "Joining strings together",
            "Python takes two or more strings and creates a brand new string that contains all of them combined in order." };
        case 2: return { "String concatenation — new allocation",
            "Python allocates a new string object large enough to hold both strings, copies the characters from each into the new object, and returns a pointer to it. The original strings are unchanged (strings are immutable)." };
        case 3: return { "PyUnicode_Concat → PyObject_Malloc",
            "PyUnicode_Concat: checks kinds, allocates new PyUnicodeObject of combined length via PyObject_Malloc(sizeof(PyUnicodeObject) + len). memcpy both string buffers in. INCREF result, DECREF operands." };
        default: return { "Allocation + copy at address level",
            "malloc(len_a + len_b + 1)    ; allocate result buffer\nmemcpy(dst, a, len_a)        ; copy first string\nmemcpy(dst+len_a, b, len_b)  ; copy second string\ndst[len_a+len_b] = '\\0'     ; null-terminate\nx = ptr to new PyUnicodeObject" };
        }
    }
    return {};
}

// ============================================================================
// Pattern: list operations
// ============================================================================
MachineExplanation MachineViewPanel::tryListOp(const QString& line, const QString& lang, int level) const
{
    bool isAppend = line.contains(".append(");
    bool isLen    = line.contains("len(");
    bool isListLit= line.contains("= [");
    if (!isAppend && !isLen && !isListLit) return {};

    if (lang == "python") {
        if (isAppend) {
            switch (level) {
            case 1: return { "Adding an item to a list",
                "Python adds the item to the end of the list. The list grows to hold the new item." };
            case 2: return { "list.append — amortized O(1)",
                "If the list has spare capacity, the item pointer is written at the next slot and the count is incremented. If capacity is full, Python reallocates the internal array at ~1.125x growth, copies existing pointers, then appends." };
            case 3: return { "list_append_impl → list_resize",
                "PyList_Append calls list_resize(). If ob_size < allocated: ob_item[ob_size++] = item; INCREF(item). Otherwise: realloc(ob_item, new_size * sizeof(PyObject*)); copy; append." };
            default: return { "list.append at pointer level",
                "if list->ob_size < list->allocated:\n  list->ob_item[list->ob_size] = ptr_to_item\n  INCREF(ptr_to_item)\n  list->ob_size += 1\nelse:\n  realloc(list->ob_item, new_capacity * 8)\n  copy + append" };
            }
        }
        if (isLen) {
            switch (level) {
            case 1: return { "Getting the length",
                "Python counts how many items are in the collection and gives back that number." };
            case 2: return { "len() — O(1) lookup",
                "For lists, tuples, and strings, len() reads the pre-stored ob_size field from the object header. No counting is done — the size is always kept up to date." };
            case 3: return { "mp_length / sq_length slot",
                "len(ob) → PyObject_Size(ob) → ob->ob_type->tp_as_sequence->sq_length(ob). For PyListObject: returns ((PyVarObject*)op)->ob_size directly." };
            default: return { "len as field read",
                "mov rax, [list_ptr + ob_size_offset]  ; read ob_size field\n; rax now holds the length (integer)" };
            }
        }
        if (isListLit) {
            switch (level) {
            case 1: return { "Creating a list",
                "Python makes a new list and fills it with the items you specified inside the brackets." };
            case 2: return { "List literal allocation",
                "Python creates a new PyListObject, allocates an internal array of PyObject* pointers for the initial items, stores a reference to each item, and sets ob_size." };
            case 3: return { "BUILD_LIST opcode",
                "BUILD_LIST N: allocates PyListObject via PyList_New(N). Loops N times, popping TOS and storing pointer into ob_item[i], INCREFing each. Sets ob_size=N, allocated=N." };
            default: return { "List allocation",
                "PyListObject* list = malloc(sizeof(PyListObject))\nlist->ob_item = malloc(N * sizeof(PyObject*))\nfor i in 0..N: list->ob_item[i] = item_ptr[i]; INCREF\nlist->ob_size = N" };
            }
        }
    }
    return {};
}

// ============================================================================
// Pattern: arithmetic
// ============================================================================
MachineExplanation MachineViewPanel::tryArithmetic(const QString& line, const QString& lang, int level) const
{
    bool hasOp = line.contains('+') || line.contains('-') || line.contains('*')
                 || line.contains('/') || line.contains('%');
    bool hasAssign = line.contains('=');
    if (!hasOp || !hasAssign) return {};
    // Exclude string concat already handled
    if (line.contains('"') || line.contains('\'')) return {};

    if (lang == "python") {
        switch (level) {
        case 1: return { "Doing arithmetic",
            "Python calculates the math expression on the right and stores the result in the variable on the left." };
        case 2: return { "Integer arithmetic — PyObject",
            "Python integers are objects. For small integers (-5 to 256), Python reuses pre-allocated singletons. For others, it may allocate a new PyLongObject. The arithmetic operator dispatches through the number protocol (nb_add, nb_mul, etc.)." };
        case 3: return { "BINARY_OP opcode + nb_add",
            "BINARY_OP: pops two operands from the stack, calls ob_type->tp_as_number->nb_add(a, b). For int: PyLong_AsLong if fits, else BigInt path. Result INCREF'd and pushed. DECREF both operands." };
        default: return { "Arithmetic at object level",
            "LOAD a → push ptr_a\nLOAD b → push ptr_b\nnb_add(ptr_a, ptr_b) → new PyLongObject\nINCREF result; DECREF ptr_a; DECREF ptr_b\nSTORE result → local variable table[idx]" };
        }
    } else {
        switch (level) {
        case 1: return { "Doing arithmetic",
            "The processor computes the math and puts the result in the variable." };
        case 2: return { "CPU arithmetic instruction",
            "The compiler translates the expression into one or more ADD, SUB, IMUL, IDIV instructions. Values are loaded into CPU registers, the operation is performed, and the result is stored back to the variable's stack slot or register." };
        case 3: return { "ALU operation in registers",
            "Values loaded from memory into registers (MOV rax, [rbp-offset]). ALU performs the operation in a single clock cycle (ADD/SUB) or ~3 cycles (IMUL) or ~20-40 cycles (IDIV). Result stored via MOV [rbp-offset], rax." };
        default: return { "Assembly arithmetic",
            "mov eax, [var_a]   ; load a\nadd eax, [var_b]   ; a + b  (or sub/imul/idiv)\nmov [result], eax  ; store result" };
        }
    }
}

// ============================================================================
// Pattern: function call
// ============================================================================
MachineExplanation MachineViewPanel::tryFunctionCall(const QString& line, const QString& lang, int level) const
{
    static QRegularExpression reCall(R"([A-Za-z_][A-Za-z0-9_.]*\()");
    if (!reCall.match(line).hasMatch()) return {};
    // Don't double-match print (already handled)
    if (line.startsWith("print(")) return {};

    if (lang == "python") {
        switch (level) {
        case 1: return { "Calling a function",
            "Python runs the code inside the named function, using the values you put in the brackets, and may give back a result." };
        case 2: return { "Function call — new frame",
            "Python looks up the function object, creates a new stack frame with the function's local variable table, copies arguments into it, and starts executing the function's bytecode. When done, the frame is removed and the return value is left on the stack." };
        case 3: return { "CALL opcode → _PyEval_EvalFrameDefault",
            "CALL: pushes a new PyFrameObject onto the call stack. _PyEval_EvalFrameDefault enters a new eval loop iteration with the function's code object. Arguments placed into fast locals (LOAD_FAST slots). On return, PyFrameObject is freed/recycled." };
        default: return { "Frame creation at address level",
            "PyFrameObject* frame = _PyFrame_New_NoTrack(tstate, code, globals, locals)\nframe->f_locals[0..n] = arg_ptrs[0..n]\nINCREF each arg\npush frame onto tstate->frame_stack\ncall PyEval_EvalFrameEx(frame)" };
        }
    } else {
        switch (level) {
        case 1: return { "Calling a function",
            "The program jumps to the function's code, runs it with the given inputs, and comes back to continue from here." };
        case 2: return { "Function call — stack frame",
            "Arguments are pushed onto the stack (or placed in registers per the calling convention). A new stack frame is created (RSP adjusted). The CALL instruction pushes the return address and jumps to the function's code." };
        case 3: return { "CALL instruction + calling convention",
            "System V AMD64: first 6 integer args in RDI, RSI, RDX, RCX, R8, R9; rest on stack. CALL pushes RIP+1 onto stack, JMPs to function. Callee: PUSH RBP; MOV RBP, RSP; SUB RSP, frame_size." };
        default: return { "CALL instruction",
            "; Place arguments\nmov edi, arg1    ; first arg in RDI\nmov esi, arg2    ; second arg in RSI\ncall function    ; push return addr, jump\n; return value in RAX" };
        }
    }
}

// ============================================================================
// Pattern: variable assignment
// ============================================================================
MachineExplanation MachineViewPanel::tryVariableAssignment(const QString& line, const QString& lang, int level) const
{
    if (!line.contains('=') || line.contains("==") || line.startsWith("if")
        || line.startsWith("while") || line.startsWith("for")) return {};
    if (!line.contains('=')) return {};

    QString varName = line.left(line.indexOf('=')).trimmed().remove('*').trimmed();
    if (varName.isEmpty() || varName.contains(' ') || varName.contains('(')) return {};

    if (lang == "python") {
        switch (level) {
        case 1: return { "Storing a value in a variable",
            QString("Python creates a value from the right side and labels it '%1'. From now on, '%1' refers to that value.").arg(varName) };
        case 2: return { "Variable binding",
            QString("Python evaluates the right-hand expression to produce a Python object. The variable '%1' is a name in the current namespace (local variable table or global dict). Assigning makes '%1' point to the new object. The old object's reference count is decremented.").arg(varName) };
        case 3: return { "STORE_FAST opcode",
            QString("STORE_FAST %1: pops TOS (the new object), INCREFs it, stores its pointer into fastlocals[index]. DECREFs the old object that was in fastlocals[index]. If old refcount reaches 0, calls tp_dealloc.").arg(varName) };
        default: return { "Assignment at memory level",
            QString("old_ptr = frame->fastlocals[%1_idx]\nframe->fastlocals[%1_idx] = new_ptr\nINCREF(new_ptr)\nDECREF(old_ptr)  ; if refcount==0: free(old_ptr)").arg(varName) };
        }
    } else {
        switch (level) {
        case 1: return { "Storing a value",
            QString("The computer puts the value into the variable '%1' in memory.").arg(varName) };
        case 2: return { "Memory write",
            QString("The right-hand value is computed (possibly loading from memory or a register). The result is written to '%1's storage location — either a register (if optimized) or a stack slot at a fixed offset from the base pointer.").arg(varName) };
        case 3: return { "MOV to stack slot",
            QString("If '%1' is a local variable: MOV [rbp - offset], value. If global: MOV [global_addr], value. Compiler may keep it in a register and never write to memory if it proves no aliasing.").arg(varName) };
        default: return { "MOV instruction",
            QString("mov [rbp - %1_offset], rax  ; store rhs result into %1's stack slot").arg(varName) };
        }
    }
}

// ============================================================================
// C-specific patterns
// ============================================================================

MachineExplanation MachineViewPanel::tryCDeclaration(const QString& line, const QString& lang, int level) const
{
    if (lang == "python") return {};
    // Match: type name; or type name = ...;
    static QRegularExpression reDecl(R"(^(int|char|float|double|long|short|unsigned|void|size_t|uint\w*|int\w*)\s+\*?[A-Za-z_])");
    if (!reDecl.match(line).hasMatch()) return {};
    if (line.contains('(') && !line.contains('=')) return {}; // function decl

    switch (level) {
    case 1: return { "Declaring a variable",
        "The program reserves a small slot of memory and gives it a name. The type tells the computer how many bytes to use." };
    case 2: return { "Stack allocation",
        "Local variables are allocated on the call stack. The compiler calculates the total size of all locals and subtracts that amount from RSP (stack pointer) at the function's prologue. Each variable gets a fixed offset from RBP." };
    case 3: return { "Stack frame slot assignment",
        "During compilation: variable assigned to [rbp - N] slot. At runtime: the slot already exists (RSP was decremented at prologue). If initialised, a MOV stores the initial value. No separate allocation call needed." };
    default: return { "Stack slot",
        "Compiler assigns rbp-offset. At runtime:\nsub rsp, frame_size       ; done once in prologue\n; variable lives at [rbp - N] for its lifetime\nmov [rbp - N], initial_val  ; if initialised" };
    }
}

MachineExplanation MachineViewPanel::tryCPointer(const QString& line, const QString& lang, int level) const
{
    if (lang == "python") return {};
    bool hasStar = line.contains('*') && !line.contains("/*") && !line.contains("//");
    bool hasAmp  = line.contains('&');
    if (!hasStar && !hasAmp) return {};
    if (!line.contains('=') && !line.contains("->")) return {};

    switch (level) {
    case 1: return { "Working with a pointer / address",
        "A pointer holds the memory address of another value. Dereferencing (*) reads or writes the value at that address. & gives you the address of a variable." };
    case 2: return { "Pointer indirection",
        "The pointer variable holds a memory address (a number). Dereferencing loads that address into a register, then reads/writes the memory at that location. & (address-of) loads the variable's own address into a register." };
    case 3: return { "MOV with memory indirection",
        "Load pointer: MOV rax, [rbp - ptr_offset]  (loads address stored in pointer)\nDereference read: MOV rbx, [rax]  (reads 4/8 bytes at the address)\nDereference write: MOV [rax], value\n& (address-of): LEA rax, [rbp - var_offset]" };
    default: return { "Pointer ops in assembly",
        "lea rax, [rbp - var]   ; & — load effective address\nmov rbx, [rax]         ; * — dereference (read)\nmov [rax], rcx         ; * — dereference (write)\nmov rax, [rbp - ptr]   ; load pointer value\nmov rdx, [rax + 8]     ; ptr->field (8-byte offset)" };
    }
}

MachineExplanation MachineViewPanel::tryCMalloc(const QString& line, const QString& lang, int level) const
{
    if (lang == "python") return {};
    if (!line.contains("malloc(") && !line.contains("calloc(") && !line.contains("realloc(")) return {};

    switch (level) {
    case 1: return { "Asking for memory from the heap",
        "malloc asks the operating system for a block of memory of the requested size. It gives back a pointer to that block, which you must free later." };
    case 2: return { "Heap allocation",
        "malloc looks in its free-list for a block of the requested size. If found, it splits/returns it. If not, it asks the OS for more memory via sbrk() or mmap(). The returned pointer points to usable memory; the bookkeeping is stored just before it." };
    case 3: return { "glibc malloc → ptmalloc2",
        "malloc(n): rounds up to alignment boundary. Checks tcache (thread-local fast bins) first — O(1) pop. Falls back to fastbins, smallbins, unsorted bin, then top chunk. Top chunk extends via sbrk(). Returns ptr to user data (chunk_start + 16)." };
    default: return { "malloc at address level",
        "CALL malloc@PLT\n; arg: rdi = requested_bytes\n; internally: check tcache → bins → top_chunk → sbrk()\n; return: rax = ptr to allocated block\n; [rax - 8] = chunk size + flags (bookkeeping)\nmov [rbp - ptr_offset], rax  ; save pointer" };
    }
}

MachineExplanation MachineViewPanel::tryCFree(const QString& line, const QString& lang, int level) const
{
    if (lang == "python") return {};
    if (!line.startsWith("free(")) return {};

    switch (level) {
    case 1: return { "Releasing memory back to the heap",
        "free gives the memory block back so it can be reused. After calling free, you should not use the pointer again." };
    case 2: return { "Heap deallocation",
        "free reads the chunk metadata stored just before the pointer to find the block's size. It then places the block back into the appropriate free-list bin. No memory is actually returned to the OS unless the top chunk shrinks past a threshold." };
    case 3: return { "free → return to bin",
        "free(ptr): reads chunk header at ptr-16 for size+flags. If size fits tcache: push to tcache list. Else merge with adjacent free chunks (coalescing). Insert into unsorted bin or fastbin. Optionally call sbrk(-n) to return to OS." };
    default: return { "free at address level",
        "CALL free@PLT\n; arg: rdi = ptr_to_free\n; reads [rdi - 8] for chunk size\n; inserts chunk into tcache/fastbin/unsorted bin\n; may coalesce with neighbours\n; ptr is now invalid — do NOT use it" };
    }
}

MachineExplanation MachineViewPanel::tryCArray(const QString& line, const QString& lang, int level) const
{
    if (lang == "python") return {};
    if (!line.contains('[') || !line.contains(']')) return {};
    if (line.startsWith("//") || line.startsWith("if") || line.startsWith("while")) return {};

    switch (level) {
    case 1: return { "Accessing an array element",
        "The computer jumps to the correct slot in the array (numbered from 0) and reads or writes the value there." };
    case 2: return { "Array indexing — pointer arithmetic",
        "An array name is a pointer to the first element. arr[i] is exactly *(arr + i). The index is multiplied by the element size (e.g., 4 for int) and added to the base address to get the element's address." };
    case 3: return { "Address computation",
        "lea rax, [arr_base]          ; base address\nimul rbx, index, elem_size   ; index * sizeof(element)\nadd rax, rbx                 ; final address\nmov ecx, [rax]               ; read element (or mov [rax], val to write)" };
    default: return { "Array access in assembly",
        "Base address in register (e.g., rdi = &arr[0])\nIndex scaled: lea rax, [rdi + rsi*4]  ; for int array (4 bytes)\nRead: mov eax, [rax]           ; loads 4 bytes\nWrite: mov [rdi + rsi*4], ecx  ; stores 4 bytes" };
    }
}
