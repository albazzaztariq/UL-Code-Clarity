#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Utility that loads JSON data files bundled with the application.
// Files are expected under a "data/" directory relative to the executable.
class JsonLoader
{
public:
    // Load a top-level array from a JSON file in data/.
    // fileName: e.g. "tutorials.json"
    // key: top-level key whose value is the array; pass "" to load the root array.
    static QJsonArray  loadArray(const QString& fileName, const QString& key = "");

    // Load the root object from a JSON file in data/.
    static QJsonObject loadObject(const QString& fileName);

private:
    static QString    dataPath(const QString& fileName);
    static QJsonObject parseFile(const QString& filePath);
};
