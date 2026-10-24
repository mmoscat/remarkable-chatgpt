#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <stdexcept>
// Anthropic Messages API responses: {"type":"message","stop_reason":...,"content":[blocks]}.
inline void diaryCheckStop(const QJsonObject &root) {
    if(root.value("type").toString()=="error" || root.contains("error"))
        throw std::runtime_error("API returned an error");
    const QString stop=root.value("stop_reason").toString();
    if(stop=="max_tokens") throw std::runtime_error("Answer incomplete; try a shorter question");
    if(stop=="refusal") throw std::runtime_error("The model declined to answer this page");
    if(stop=="pause_turn") throw std::runtime_error("Search took too long; please try again");
    if(stop!="end_turn" && stop!="stop_sequence" && stop!="tool_use")
        throw std::runtime_error("Answer incomplete; try again");
}
inline QString responseText(const QJsonObject &root) {
    diaryCheckStop(root);
    QString text;
    for(auto value:root.value("content").toArray()) {
        auto item=value.toObject();
        if(item.value("type").toString()=="text") text+=item.value("text").toString();
    }
    if(text.trimmed().isEmpty() || text.size()>16000)
        throw std::runtime_error("Empty or oversized answer");
    return text.trimmed();
}
// Structured answers arrive as the input of a forced tool call; return it as compact JSON text
// so decodeDiaryAnswer() can parse it unchanged.
inline QString diaryToolJson(const QJsonObject &root) {
    diaryCheckStop(root);
    for(auto value:root.value("content").toArray()) {
        auto item=value.toObject();
        if(item.value("type").toString()=="tool_use" && item.value("name").toString()=="ink_diary_answer" && item.value("input").isObject()) {
            QString json=QString::fromUtf8(QJsonDocument(item.value("input").toObject()).toJson(QJsonDocument::Compact));
            if(json.size()>16000) throw std::runtime_error("Empty or oversized answer");
            return json;
        }
    }
    throw std::runtime_error("Could not read structured answer; please try again");
}
