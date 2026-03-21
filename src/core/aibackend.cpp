#include "core/aibackend.h"

#include <QSettings>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

AIBackend::AIBackend(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// ── Public ────────────────────────────────────────────────────────────────

void AIBackend::sendMessage(const QString &userMessage,
                            const QString &fileContent,
                            const QString &language,
                            int level)
{
    // Abort any previous in-flight request
    abort();

    m_buffer.clear();
    m_fullResponse.clear();
    m_emittedCodeBlocks = 0;

    // Read settings
    QSettings s("CodeClarity", "CodeClarity");
    m_provider        = s.value("ai/provider",   "Anthropic").toString();
    QString endpoint  = s.value("ai/endpoint",   "").toString();
    QString apiKey    = s.value("ai/apiKey",     "").toString();
    QString modelName = s.value("ai/modelName",  "").toString();

    // Validate
    if (apiKey.isEmpty() && m_provider != "Ollama") {
        emit errorOccurred("No API key configured. Open the gear icon to set up your AI model.");
        return;
    }

    // Fill default endpoints / model names if user left them blank
    if (endpoint.isEmpty()) {
        if (m_provider == "Anthropic")
            endpoint = "https://api.anthropic.com/v1/messages";
        else if (m_provider == "OpenAI")
            endpoint = "https://api.openai.com/v1/chat/completions";
        else if (m_provider == "Ollama")
            endpoint = "http://localhost:11434/api/generate";
        else
            endpoint = "https://api.openai.com/v1/chat/completions"; // Custom falls back to OAI-compat
    }
    if (modelName.isEmpty()) {
        if (m_provider == "Anthropic")  modelName = "claude-sonnet-4-6";
        else if (m_provider == "OpenAI") modelName = "gpt-4o";
        else if (m_provider == "Ollama") modelName = "llama3";
        else                             modelName = "gpt-4o";
    }

    QString systemPrompt = buildSystemPrompt(language, fileContent, level);

    // Build request body
    QByteArray body;
    if (m_provider == "Anthropic") {
        body = buildAnthropicBody(systemPrompt, userMessage, modelName);
    } else if (m_provider == "Ollama") {
        body = buildOllamaBody(systemPrompt, userMessage, modelName);
    } else {
        // OpenAI-compatible (OpenAI, Google via OAI-compat, Custom)
        body = buildOpenAIBody(systemPrompt, userMessage, modelName);
    }

    // Build HTTP request
    QUrl reqUrl(endpoint);
    QNetworkRequest req(reqUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (m_provider == "Anthropic") {
        req.setRawHeader("x-api-key", apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
    } else if (m_provider != "Ollama") {
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    }

    // Enable streaming via attribute (Qt reads chunks as they arrive)
    req.setAttribute(QNetworkRequest::AutoDeleteReplyOnFinishAttribute, false);

    m_reply = m_nam->post(req, body);
    connect(m_reply, &QNetworkReply::readyRead, this, &AIBackend::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AIBackend::onFinished);
}

void AIBackend::abort()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

// ── Streaming slots ───────────────────────────────────────────────────────

void AIBackend::onReadyRead()
{
    if (!m_reply) return;

    QByteArray raw = m_reply->readAll();
    m_buffer += QString::fromUtf8(raw);

    // Process line by line (SSE lines end with \n)
    QStringList lines = m_buffer.split('\n');
    // Keep the last (potentially incomplete) line in buffer
    m_buffer = lines.takeLast();

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        QString delta;

        if (m_provider == "Anthropic") {
            delta = parseAnthropicSSE(trimmed);
        } else if (m_provider == "Ollama") {
            delta = parseOllamaChunk(trimmed);
        } else {
            delta = parseOpenAISSE(trimmed);
        }

        if (!delta.isEmpty()) {
            m_fullResponse += delta;
            emit responseChunk(delta);
            detectCodeBlocks();
        }
    }
}

void AIBackend::onFinished()
{
    if (!m_reply) return;

    if (m_reply->error() != QNetworkReply::NoError &&
        m_reply->error() != QNetworkReply::OperationCanceledError)
    {
        // Try to extract error message from response body
        QByteArray errBody = m_reply->readAll();
        QString errMsg = m_reply->errorString();
        if (!errBody.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(errBody);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                // Anthropic error format
                if (obj.contains("error")) {
                    QJsonObject errObj = obj["error"].toObject();
                    if (errObj.contains("message"))
                        errMsg = errObj["message"].toString();
                }
                // OpenAI error format
                else if (obj.contains("message")) {
                    errMsg = obj["message"].toString();
                }
            }
        }
        emit errorOccurred(errMsg);
    } else {
        // Process any remaining buffered data
        if (!m_buffer.isEmpty()) {
            QString delta;
            if (m_provider == "Anthropic")
                delta = parseAnthropicSSE(m_buffer.trimmed());
            else if (m_provider == "Ollama")
                delta = parseOllamaChunk(m_buffer.trimmed());
            else
                delta = parseOpenAISSE(m_buffer.trimmed());

            if (!delta.isEmpty()) {
                m_fullResponse += delta;
                emit responseChunk(delta);
                detectCodeBlocks();
            }
            m_buffer.clear();
        }

        emit responseComplete();
    }

    m_reply->deleteLater();
    m_reply = nullptr;
}

// ── Request builders ──────────────────────────────────────────────────────

// Returns the level-specific system prompt from CC-AssistLevels.md definitions.
QString AIBackend::levelSystemPrompt(int level)
{
    switch (level) {
    case 1:
        return QStringLiteral(
            "You are explaining code to someone who has NEVER written code before. "
            "For every line, explain what it does using everyday language. "
            "When you encounter a function, say 'this is a reusable block of code called [name]'. "
            "When you encounter a variable, say 'this creates a container called [name] that holds [type]'. "
            "When you encounter a loop, say 'this repeats the following steps for each [item]'. "
            "Define EVERY programming concept the first time it appears. "
            "Use analogies to real life. "
            "Never use the words: scope, heap, stack, reference, pointer, runtime, compiler, "
            "iterator, lambda, closure, callback, API, instance, polymorphism, encapsulation, abstraction. "
            "If you must reference one of these concepts, describe what it DOES without naming it."
        );
    case 2:
        return QStringLiteral(
            "You are explaining code to someone who knows the basics: variables, functions, loops, "
            "if/else, arrays, strings, and printing. Do NOT explain what these are. "
            "Focus on what THIS code does specifically — what each function's purpose is, "
            "what data flows where, and why the code is structured this way. "
            "When you encounter error handling, classes, inheritance, or file I/O, briefly explain "
            "what's happening. "
            "Do NOT use these terms without a one-sentence explanation: closure, decorator, generator, "
            "async, thread, garbage collection, polymorphism, encapsulation. "
            "Never explain what a variable, function, loop, or conditional is."
        );
    case 3:
        return QStringLiteral(
            "You are explaining code to an experienced developer who is seeing this codebase for the "
            "first time. They know every programming concept — do NOT define any terms. Be terse. "
            "Focus on: what each module/file does, how they connect, the call graph, any architectural "
            "patterns (MVC, observer, factory, etc), any non-obvious design decisions, and potential "
            "gotchas. Use technical terms freely. "
            "Think of this as a senior engineer's codebase walkthrough."
        );
    case 4:
        return QStringLiteral(
            "List all functions with their parameter types and return types. "
            "List all classes/types with their fields. "
            "List the entry point. Nothing else. No explanations."
        );
    default:
        return levelSystemPrompt(1);
    }
}

QString AIBackend::buildSystemPrompt(const QString &language, const QString &fileContent, int level) const
{
    // Start with the level-specific persona prompt
    QString prompt = levelSystemPrompt(level);

    // Append file context
    prompt += QString("\n\nThe user is editing a %1 file.")
                  .arg(language.isEmpty() ? "code" : language);

    if (!fileContent.isEmpty()) {
        prompt += QString("\n\nCurrent file content:\n%1").arg(fileContent);
    }

    prompt += QString(
        "\n\nWhen you want to edit code, wrap it in ```%1\n...\n``` blocks. "
        "The editor will apply your changes."
    ).arg(language.isEmpty() ? "code" : language);

    return prompt;
}

QByteArray AIBackend::buildAnthropicBody(const QString &systemPrompt,
                                         const QString &userMessage,
                                         const QString &model) const
{
    QJsonObject body;
    body["model"]      = model;
    body["max_tokens"] = 4096;
    body["stream"]     = true;
    body["system"]     = systemPrompt;

    QJsonArray messages;
    QJsonObject msg;
    msg["role"]    = "user";
    msg["content"] = userMessage;
    messages.append(msg);
    body["messages"] = messages;

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray AIBackend::buildOpenAIBody(const QString &systemPrompt,
                                      const QString &userMessage,
                                      const QString &model) const
{
    QJsonObject body;
    body["model"]  = model;
    body["stream"] = true;

    QJsonArray messages;

    QJsonObject sysMsg;
    sysMsg["role"]    = "system";
    sysMsg["content"] = systemPrompt;
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    body["messages"] = messages;

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray AIBackend::buildOllamaBody(const QString &systemPrompt,
                                      const QString &userMessage,
                                      const QString &model) const
{
    QJsonObject body;
    body["model"]  = model;
    body["prompt"] = systemPrompt + "\n\nUser: " + userMessage;
    body["stream"] = true;

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

// ── SSE parsers ───────────────────────────────────────────────────────────

// Anthropic SSE lines look like:
//   data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"Hello"}}
QString AIBackend::parseAnthropicSSE(const QString &line) const
{
    if (!line.startsWith("data: ")) return {};
    QString json = line.mid(6);
    if (json == "[DONE]") return {};

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return {};

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "content_block_delta") {
        QJsonObject delta = obj["delta"].toObject();
        if (delta["type"].toString() == "text_delta")
            return delta["text"].toString();
    }
    return {};
}

// OpenAI SSE lines look like:
//   data: {"choices":[{"delta":{"content":"Hello"}}]}
QString AIBackend::parseOpenAISSE(const QString &line) const
{
    if (!line.startsWith("data: ")) return {};
    QString json = line.mid(6);
    if (json == "[DONE]") return {};

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return {};

    QJsonObject obj = doc.object();
    QJsonArray choices = obj["choices"].toArray();
    if (choices.isEmpty()) return {};

    QJsonObject delta = choices[0].toObject()["delta"].toObject();
    return delta["content"].toString();
}

// Ollama streaming: each line is a full JSON object
//   {"model":"llama3","response":"Hello","done":false}
QString AIBackend::parseOllamaChunk(const QString &line) const
{
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
    if (!doc.isObject()) return {};

    QJsonObject obj = doc.object();
    return obj["response"].toString();
}

// ── Code block detection ──────────────────────────────────────────────────

void AIBackend::detectCodeBlocks()
{
    // Find completed fenced code blocks: ```lang\n...\n```
    // We count how many complete blocks exist and emit only new ones.
    static const QRegularExpression re(
        "```([\\w+-]*)\\n([\\s\\S]*?)\\n```",
        QRegularExpression::MultilineOption
    );

    QRegularExpressionMatchIterator it = re.globalMatch(m_fullResponse);
    int count = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        count++;
        if (count > m_emittedCodeBlocks) {
            QString lang = match.captured(1);
            QString code = match.captured(2);
            emit codeBlock(code, lang);
            m_emittedCodeBlocks = count;
        }
    }
}
