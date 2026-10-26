#pragma once
#include "MeteoUiExport.hpp"
#include <QByteArray>
#include <QUrl>
#include <QVariantList>
#include <optional>

class METEO_UI_EXPORT ProfileFileCodec
{
public:
    static constexpr qsizetype maximumBytes = 16 * 1024 * 1024;
    static std::optional<QVariantList> decode(const QByteArray &bytes, QString *error = nullptr);
    static std::optional<QByteArray> encode(const QVariantList &profiles, QString *error = nullptr);
    static std::optional<QVariantList> read(const QUrl &file, QString *error = nullptr);
    static bool write(const QUrl &file, const QVariantList &profiles, QString *error = nullptr);
};
