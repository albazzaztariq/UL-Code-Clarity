#include "core/jsonloader.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

QString JsonLoader::dataPath(const QString& fileName)
{
    // First try next to the executable (production)
    QString exeDir = QApplication::applicationDirPath();
    QString path   = exeDir + "/data/" + fileName;
    if (QFile::exists(path))
        return path;

    // Fall back to the src/data/ directory (development / Qt Creator run)
    // Walk up from exe dir to find the data folder
    QDir dir(exeDir);
    for (int i = 0; i < 6; ++i) {
        QString candidate = dir.filePath("src/data/" + fileName);
        if (QFile::exists(candidate))
            return candidate;
        if (!dir.cdUp())
            break;
    }

    qWarning() << "JsonLoader: could not find data file:" << fileName;
    return QString();
}

QJsonObject JsonLoader::parseFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return {};

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "JsonLoader: failed to open" << filePath;
        return {};
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JsonLoader: parse error in" << filePath << ":" << err.errorString();
        return {};
    }

    if (!doc.isObject()) {
        qWarning() << "JsonLoader: root is not an object in" << filePath;
        return {};
    }

    return doc.object();
}

QJsonObject JsonLoader::loadObject(const QString& fileName)
{
    return parseFile(dataPath(fileName));
}

QJsonArray JsonLoader::loadArray(const QString& fileName, const QString& key)
{
    QJsonObject root = parseFile(dataPath(fileName));
    if (root.isEmpty())
        return {};

    if (key.isEmpty()) {
        // Caller expects the root itself to be an array wrapped in an object?
        // Return empty — callers should supply a key.
        qWarning() << "JsonLoader::loadArray: key is empty for" << fileName;
        return {};
    }

    QJsonValue val = root.value(key);
    if (!val.isArray()) {
        qWarning() << "JsonLoader: key" << key << "is not an array in" << fileName;
        return {};
    }

    return val.toArray();
}
