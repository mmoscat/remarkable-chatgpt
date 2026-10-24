#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QEventLoop>
#include <QTimer>
#include <functional>
#include <stdexcept>
// Anthropic Messages API client (https://api.anthropic.com/v1/messages).
inline QJsonObject diaryApiRequest(const QJsonObject &payload,const QString &key,
                                  const std::function<bool()> &valid) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setRawHeader("x-api-key", key.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=manager.post(request,QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop; QTimer timeout,cancelPoll;
    timeout.setSingleShot(true);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
    QObject::connect(&cancelPoll,&QTimer::timeout,reply,[&]{if(!valid())reply->abort();});
    const qint64 limit=1024*1024;
    QByteArray bytes;bool oversized=false;
    auto drain=[&] { auto chunk=reply->readAll();if(bytes.size()+chunk.size()>limit){oversized=true;reply->abort();}else bytes+=chunk; };
    reply->setReadBufferSize(65536);
    QObject::connect(reply,&QNetworkReply::readyRead,reply,drain);
    timeout.start(180000);cancelPoll.start(300);loop.exec();
    if(!oversized)drain();
    if(oversized)throw std::runtime_error("Response too large");
    if(!valid())throw std::runtime_error("Cancelled or page changed");
    int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(code!=200||reply->error()!=QNetworkReply::NoError) {
        // Show only the short machine-readable error type (e.g. not_found_error), never free text.
        QString kind=QJsonDocument::fromJson(bytes).object().value("error").toObject().value("type").toString();
        if(!QRegularExpression("^[a-z_]{1,40}$").match(kind).hasMatch())kind.clear();
        throw std::runtime_error(QString("Anthropic request failed (HTTP %1%2); check Wi-Fi, key, model and web-search access")
            .arg(code).arg(kind.isEmpty()?QString():", "+kind).toStdString());
    }
    auto doc=QJsonDocument::fromJson(bytes);
    if(!doc.isObject())throw std::runtime_error("Invalid API response");
    return doc.object();
}
