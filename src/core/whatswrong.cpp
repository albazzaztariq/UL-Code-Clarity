#include "core/whatswrong.h"
#include "core/jsonloader.h"
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>

WhatsWrongAnalyzer::WhatsWrongAnalyzer(QObject* parent)
    : QObject(parent)
{
    // Load error type descriptors from data/errors.json
    QJsonArray arr = JsonLoader::loadArray("errors.json", "error_types");
    for (const QJsonValue& v : arr)
        m_errorTypes.append(v.toObject());
}

QList<PossibleCause> WhatsWrongAnalyzer::analyzeError(const QString& errorMessage,
                                                        const QString& /*code*/,
                                                        int crashLine) const
{
    QList<PossibleCause> causes;
    QString msg = errorMessage.toLower();

    for (const QJsonObject& et : m_errorTypes) {
        // Check if this error type matches the message
        bool matched = false;
        if (et.contains("match")) {
            matched = msg.contains(et["match"].toString());
        } else if (et.contains("match_any")) {
            for (const QJsonValue& m : et["match_any"].toArray()) {
                if (msg.contains(m.toString())) { matched = true; break; }
            }
        }
        if (!matched) continue;

        PossibleCause c;
        c.title        = et["title"].toString();
        c.relevantLine = crashLine;

        // Try specific regex patterns first
        bool patternMatched = false;
        if (et.contains("patterns")) {
            for (const QJsonValue& pv : et["patterns"].toArray()) {
                QJsonObject p = pv.toObject();
                QRegularExpression re(p["regex"].toString());
                auto m = re.match(errorMessage);
                if (m.hasMatch()) {
                    // Build explanation — substitute %1, %2, %3 captures
                    QString expl = p["explanation_template"].toString();
                    QString fix  = p.contains("fix") ? p["fix"].toString()
                                                     : p["fix_template"].toString();
                    for (int i = 1; i <= m.lastCapturedIndex(); ++i) {
                        expl.replace(QString("%%1").arg(i), m.captured(i));
                        fix.replace(QString("%%1").arg(i), m.captured(i));
                    }
                    c.explanation  = expl;
                    c.suggestedFix = fix;
                    patternMatched = true;
                    break;
                }
            }
        }
        // Also check single top-level regex (name_error style)
        if (!patternMatched && et.contains("regex")) {
            QRegularExpression re(et["regex"].toString());
            auto m = re.match(errorMessage);
            if (m.hasMatch()) {
                QString captured = m.captured(et["capture_group"].toInt(1));
                c.explanation  = et["explanation_with_var"].toString().replace("%1", captured);
                c.suggestedFix = et["fix_with_var"].toString().replace("%1", captured);
                patternMatched = true;
            }
        }

        if (!patternMatched) {
            c.explanation  = et["explanation_generic"].toString();
            c.suggestedFix = et["fix_generic"].toString();
        }

        causes.append(c);
    }

    return causes;
}
