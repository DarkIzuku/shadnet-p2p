// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QUrl>

class QHttpServerRequest;
class QHttpServerResponder;

namespace Bloodborne {

class ReferenceProxy final : public QObject {
public:
    struct Options {
        QUrl upstreamUrl;
        QString captureRoot = QStringLiteral("captures/bloodborne-reference");
        int transferTimeoutMs = 15000;
    };

    explicit ReferenceProxy(Options options, QObject* parent = nullptr);
    ~ReferenceProxy() override;

    bool Initialize(QString* error);
    void Forward(const QHttpServerRequest& request, QHttpServerResponder&& responder);

    QString CaptureDirectory() const;
    static bool IsReferenceBackendPath(const QString& path);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Bloodborne
