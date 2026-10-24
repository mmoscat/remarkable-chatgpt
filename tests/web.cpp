#include "../src/webanswer.h"
#include "../src/plot.h"
#include "../src/freshness.h"
#include <QGuiApplication>
#include <QFile>
#include <QFontDatabase>
#include <cassert>
#include <iostream>
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);
 auto example=QDateTime::fromString("2026-10-20T01:42:00Z",Qt::ISODate);
 auto local=diaryLocalClock(example);assert(local.contains("2026-10-19 09:42 PM EDT"));
 assert(local.contains("-04:00"));
 auto winter=diaryLocalClock(QDateTime::fromString("2026-12-20T02:42:00Z",Qt::ISODate));assert(winter.contains("2026-12-19 09:42 PM EST"));assert(winter.contains("-05:00"));
 auto spring=diaryLocalClock(QDateTime::fromString("2026-03-08T07:00:00Z",Qt::ISODate));assert(spring.contains("03:00 AM EDT"));
 auto fall=diaryLocalClock(QDateTime::fromString("2026-11-01T06:00:00Z",Qt::ISODate));assert(fall.contains("01:00 AM EST"));
 assert(diaryCurrentContext(example,"America/New_York").contains("2026-10-19 09:42 PM EDT"));
 for(const auto &question:{"Who is the current president?","Who is the US president today?","Who is the president of France?","What happened recently?","Search lunar eclipses"}) {
  DiaryAnswer a;a.command=question;a.text="An old answer";diaryRequireFreshSearch(a);assert(a.search&&a.text==question);
 }
 for(const auto &question:{"Who was president in 1990?","Explain electric current","Why does the Moon shine?"}) {
  DiaryAnswer a;a.command=question;diaryRequireFreshSearch(a);assert(!a.search);
 }
 DiaryAnswer graph;graph.plot=true;graph.command="plot y=x today";diaryRequireFreshSearch(graph);assert(!graph.search);
 auto timedPayload=diarySearchPayload("Weather in Lowell MA","claude-sonnet-5-5","America/New_York",example);
 assert(timedPayload["system"].toString().contains("2026-10-19 09:42 PM EDT"));
 auto payload=diarySearchPayload("Search today's weather in Boston","claude-sonnet-5-5");
 assert(payload.value("tools").toArray().at(0).toObject().value("type")=="web_search_20250305");
 assert(payload.value("tools").toArray().at(0).toObject().value("name")=="web_search");
 assert(payload.value("messages").toArray().size()==1);assert(!payload.contains("store")&&!payload.contains("input"));
 auto route=decodeDiaryAnswer(R"JSON({"kind":"search","text":"weather in Boston today"})JSON");assert(route.search&&!route.plot);
 // Narration before the final search result is dropped; cited fragments get compact markers.
 auto root=QJsonDocument::fromJson(R"JSON({"type":"message","stop_reason":"end_turn","content":[{"type":"text","text":"I'll search for that."},{"type":"server_tool_use","name":"web_search","input":{"query":"x"}},{"type":"web_search_tool_result","content":[{"type":"web_search_result","url":"https://example.org/a?x=1&y=2","title":"A <source>"}]},{"type":"text","text":"A fact.","citations":[{"type":"web_search_result_location","url":"https://example.org/a?x=1&y=2","title":"A <source>","cited_text":"fact"}]}]})JSON").object();
 auto result=parseDiarySearch(root);assert(result.text=="A fact. [1]");assert(result.sources.size()==1);
 result.checkedAt=local;assert(diarySourcesText(result).contains("Checked at: "+local));
 auto html=diarySourcesHtml(result);assert(html.contains("09:42 PM EDT"));assert(html.contains("href=\"https://example.org/a?x=1&amp;y=2\""));assert(!html.contains("A <source>"));
 assert(diarySourcesText(result).contains("https://example.org/a"));
 // No search result block at all: reject.
 bool rejected=false;
 try{parseDiarySearch(QJsonDocument::fromJson(R"JSON({"type":"message","stop_reason":"end_turn","content":[{"type":"text","text":"From memory."}]})JSON").object());}catch(...){rejected=true;}assert(rejected);
 // Tool-level error (HTTP 200 with an error object): reject.
 rejected=false;
 try{parseDiarySearch(QJsonDocument::fromJson(R"JSON({"type":"message","stop_reason":"end_turn","content":[{"type":"server_tool_use","name":"web_search","input":{"query":"x"}},{"type":"web_search_tool_result","content":{"type":"web_search_tool_result_error","error_code":"max_uses_exceeded"}},{"type":"text","text":"Guess."}]})JSON").object());}catch(...){rejected=true;}assert(rejected);
 // Search completed but nothing cited.
 root=QJsonDocument::fromJson(R"JSON({"type":"message","stop_reason":"end_turn","content":[{"type":"server_tool_use","name":"web_search","input":{"query":"x"}},{"type":"web_search_tool_result","content":[]},{"type":"text","text":"Unverified"}]})JSON").object();
 result=parseDiarySearch(root);assert(result.text=="Unverified");assert(result.sources.isEmpty());
 assert(diarySourcesText(result).contains("no source links"));assert(diarySourcesHtml(result).contains("no source links"));
 // Claude splits answers into consecutive fragments, each with its own citations; empty title falls back to host.
 root=QJsonDocument::fromJson(R"JSON({"type":"message","stop_reason":"end_turn","content":[{"type":"server_tool_use","name":"web_search","input":{"query":"x"}},{"type":"web_search_tool_result","content":[{"type":"web_search_result","url":"https://example.org","title":""}]},{"type":"text","text":"Fact one","citations":[{"type":"web_search_result_location","url":"https://example.org","title":"","cited_text":"one"}]},{"type":"text","text":" and fact two.","citations":[{"type":"web_search_result_location","url":"https://example.org","title":"","cited_text":"two"},{"type":"web_search_result_location","url":"https://example.net/b","title":"B","cited_text":"two"}]}]})JSON").object();
 result=parseDiarySearch(root);assert(result.text=="Fact one [1] and fact two. [1] [2]");assert(result.sources.size()==2);assert(result.sources[0].title=="example.org");
 if(argc>1){
  QFile input(QString::fromLocal8Bit(argv[1]));assert(input.open(QIODevice::ReadOnly));
  auto live=parseDiarySearch(QJsonDocument::fromJson(input.readAll()).object());live.checkedAt=local;
  int id=QFontDatabase::addApplicationFont("fonts/Caveat.ttf");assert(id>=0);
  QFont font(QFontDatabase::applicationFontFamilies(id).first());font.setPixelSize(48);font.setWeight(QFont::Medium);QFontMetrics fm(font);
  for(auto ch:live.text)assert(ch.isSpace()||fm.inFont(ch));
  QRect box=fm.boundingRect(QRect(0,0,1150,10000),Qt::TextWordWrap,live.text);
  QFont sourceFont("DejaVu Sans");sourceFont.setPixelSize(23);QFontMetrics sm(sourceFont);
  QString sources=diarySourcesText(live);QRect sb=sm.boundingRect(QRect(0,0,1150,10000),Qt::TextWrapAnywhere,sources);
  QImage image(1174,box.height()+54+sb.height(),QImage::Format_ARGB32);image.fill(Qt::white);QPainter p(&image);p.setPen(Qt::black);p.setFont(font);p.drawText(QRect(12,8,1150,box.height()+8),Qt::TextWordWrap,live.text);p.setFont(sourceFont);p.drawText(QRect(12,box.height()+34,1150,sb.height()+8),Qt::TextWrapAnywhere,sources);p.end();assert(image.save(argc>2?QString::fromLocal8Bit(argv[2]):QString("web-answer-preview.png")));
 }
 std::cout<<"PASS: Eastern date rollover and DST, checked-at timestamp, search routing, Anthropic web_search tool payload, cited text fragments, escaped links, missing-source notice and no-search/tool-error rejection\n";
}
