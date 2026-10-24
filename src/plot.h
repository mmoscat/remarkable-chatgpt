#pragma once
#include "templates.h"
#include <QImage>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>
#include <vector>
#include <stdexcept>

// Restricted mathematical grammar: never evaluate model output as code.
class PlotExpression {
    QString source; int pos=0; double variable=0;
    bool take(QChar c) { while(pos<source.size() && source[pos].isSpace())++pos; if(pos<source.size() && source[pos]==c){++pos;return true;}return false; }
    [[noreturn]] void bad() const {throw std::runtime_error("Unsupported equation; use y=f(x), e.g. plot y=x^2");}
    double sum(){double v=product();for(;;){if(take('+'))v+=product();else if(take('-'))v-=product();else return v;}}
    double product(){double v=unary();for(;;){if(take('*'))v*=unary();else if(take('/'))v/=unary();else return v;}}
    double unary(){if(take('+'))return unary();if(take('-'))return -unary();return power();}
    double power(){double v=atom();if(take('^'))v=std::pow(v,unary());return v;}
    double atom(){
        if(take('(')){double v=sum();if(!take(')'))bad();return v;}
        take(QChar(0));
        int start=pos;
        while(pos<source.size() && (source[pos].isDigit() || source[pos]=='.'))++pos;
        if(pos>start){bool ok=false;double v=source.mid(start,pos-start).toDouble(&ok);if(!ok)bad();return v;}
        while(pos<source.size() && source[pos].isLetter())++pos;
        QString name=source.mid(start,pos-start);
        if(name=="x")return variable;
        if(name=="pi")return std::acos(-1.0);
        if(name=="e")return std::exp(1.0);
        if(name.isEmpty() || !take('('))bad();
        double v=sum();if(!take(')'))bad();
        if(name=="sin")return std::sin(v);if(name=="cos")return std::cos(v);
        if(name=="tan")return std::tan(v);if(name=="sqrt")return std::sqrt(v);
        if(name=="abs")return std::abs(v);if(name=="exp")return std::exp(v);
        if(name=="ln" || name=="log")return std::log(v);
        bad();
    }
public:
    explicit PlotExpression(QString expression):source(expression){if(source.isEmpty()||source.size()>160)bad();eval(0);}
    double eval(double x){pos=0;variable=x;double v=sum();take(QChar(0));if(pos!=source.size())bad();return v;}
};
struct DiaryAnswer {QString text,expression,command;double xmin=-5,xmax=5;bool plot=false,search=false,image=false,calendar=false;QString diagram;int cellLines=1;};
inline DiaryAnswer decodeDiaryAnswer(const QString &raw){
    auto doc=QJsonDocument::fromJson(raw.toUtf8());
    if(!doc.isObject())throw std::runtime_error("Could not read structured answer; please try again");
    auto obj=doc.object();DiaryAnswer result;result.command=obj.value("command").toString().trimmed();
    QString kind=obj.value("kind").toString();
    if(kind=="image" || kind=="mermaid" || kind=="venn") {
        result.text=obj.value("text").toString().trimmed();
        if(result.text.isEmpty() || result.text.size()>12000)throw std::runtime_error("Visual description missing or too long");
        result.image=kind=="image";if(!result.image)result.diagram=kind;return result;
    }
    if(kind=="calendar") {result.calendar=true;result.cellLines=diaryTemplateCellLines(obj.value("template").toObject());result.text=diaryTemplateMarkdown("table",diaryCalendarTemplate(obj.value("calendar").toObject(),result.cellLines));return result;}
    if(kind=="checklist"||kind=="table") {result.text=diaryTemplateMarkdown(kind,obj.value("template").toObject());if(kind=="table")result.cellLines=diaryTemplateCellLines(obj.value("template").toObject());return result;}
    if(kind=="search") {result.search=true;result.text=obj.value("text").toString().trimmed();if(result.text.isEmpty()||result.text.size()>2000)throw std::runtime_error("Could not read search question");return result;}
    if(kind=="text") {result.text=obj.value("text").toString().trimmed();if(result.text.isEmpty())throw std::runtime_error("Empty answer");return result;}
    if(kind!="plot")throw std::runtime_error("Unknown answer format");
    // Require the recognized command as well as the classification.
    QString command=obj.value("command").toString();
    if(!QRegularExpression("^\\s*plot\\s+\\S",QRegularExpression::CaseInsensitiveOption).match(command).hasMatch())
        throw std::runtime_error("Write plot followed by an equation");
    result.plot=true;result.expression=obj.value("expression").toString();
    if(!obj.value("xmin").isDouble()||!obj.value("xmax").isDouble())throw std::runtime_error("Invalid plot range");
    result.xmin=obj.value("xmin").toDouble();result.xmax=obj.value("xmax").toDouble();
    if(!std::isfinite(result.xmin)||!std::isfinite(result.xmax)||result.xmin>=result.xmax ||
        std::abs(result.xmin)>1000000 || std::abs(result.xmax)>1000000 || result.xmax-result.xmin<0.000001)
        throw std::runtime_error("Plot range must be increasing, within +/-1000000");
    PlotExpression check(result.expression);return result;
}
inline QJsonObject diaryAnswerFormat(){
    QJsonObject props;
    for(auto name:{"kind","text","command","expression"})props.insert(name,QJsonObject{{"type","string"}});
    props.insert("kind",QJsonObject{{"type","string"},{"enum",QJsonArray{"text","plot","search","checklist","table","mermaid","venn","calendar"}}});
    props.insert("template",diaryTemplateSchema());
    props.insert("calendar",diaryCalendarSchema());
    props.insert("xmin",QJsonObject{{"type","number"}});props.insert("xmax",QJsonObject{{"type","number"}});
    return {{"type","json_schema"},{"name","ink_diary_answer"},{"strict",true},
        {"schema",QJsonObject{{"type","object"},{"properties",props},{"additionalProperties",false},
        {"required",QJsonArray{"kind","text","command","expression","xmin","xmax","template","calendar"}}}}};
}
// Claude returns the structured answer as the input of a forced tool call.
inline QJsonObject diaryAnswerTool(){
    return {{"name","ink_diary_answer"},
        {"description","Return the structured answer for the handwritten notebook page."},
        {"input_schema",diaryAnswerFormat().value("schema").toObject()}};
}
inline QImage renderPlot(const DiaryAnswer &a,int width,int available){
    if(available<480)throw std::runtime_error("Not enough room for graph; write near the top of a fresh page");
    int height=std::min(800,available);PlotExpression fn(a.expression);
    const int count=2400;std::vector<double> ys(count+1),finite;
    for(int i=0;i<=count;++i){double y=fn.eval(a.xmin+(a.xmax-a.xmin)*i/count);ys[i]=y;if(std::isfinite(y)&&std::abs(y)<1e12)finite.push_back(y);}
    if(finite.size()<2)throw std::runtime_error("No real-valued curve in this x range");
    std::sort(finite.begin(),finite.end());
    double lo=finite[finite.size()/50],hi=finite[finite.size()-1-finite.size()/50];
    double pad=(hi-lo)*0.12;if(pad<1e-8)pad=std::max(1.0,std::abs(lo)*0.1);
    lo-=pad;hi+=pad;
    QImage image(width,height,QImage::Format_ARGB32);image.fill(Qt::white);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    QFont font("DejaVu Sans");font.setPixelSize(25);p.setFont(font);p.setPen(Qt::black);
    p.drawText(QRect(20,8,width-40,55),Qt::AlignCenter,"y = "+a.expression);
    QRectF box(110,80,width-155,height-155);
    auto px=[&](double x){return box.left()+(x-a.xmin)/(a.xmax-a.xmin)*box.width();};
    auto py=[&](double y){return box.bottom()-(y-lo)/(hi-lo)*box.height();};
    for(int i=0;i<=4;++i){
        double x=a.xmin+(a.xmax-a.xmin)*i/4,y=lo+(hi-lo)*i/4;
        p.setPen(QPen(QColor(185,185,185),2));p.drawLine(QPointF(px(x),box.top()),QPointF(px(x),box.bottom()));
        p.drawLine(QPointF(box.left(),py(y)),QPointF(box.right(),py(y)));
        p.setPen(Qt::black);p.drawText(QRectF(px(x)-65,box.bottom()+10,130,32),Qt::AlignCenter,QString::number(x,'g',3));
        p.drawText(QRectF(0,py(y)-16,100,32),Qt::AlignRight|Qt::AlignVCenter,QString::number(y,'g',3));
    }
    p.setPen(QPen(Qt::black,3));p.drawRect(box);
    if(a.xmin<=0&&a.xmax>=0)p.drawLine(QPointF(px(0),box.top()),QPointF(px(0),box.bottom()));
    if(lo<=0&&hi>=0)p.drawLine(QPointF(box.left(),py(0)),QPointF(box.right(),py(0)));
    p.drawText(QRectF(box.right()-25,box.bottom()+43,35,30),Qt::AlignCenter,"x");
    p.drawText(QRectF(60,40,35,30),Qt::AlignCenter,"y");
    p.save();p.setClipRect(box);p.setPen(QPen(Qt::black,3.5));
    for(int i=1;i<=count;++i){
        double y0=ys[i-1],y1=ys[i],x0=a.xmin+(a.xmax-a.xmin)*(i-1)/count,x1=a.xmin+(a.xmax-a.xmin)*i/count;
        double mid=fn.eval((x0+x1)/2);
        // Break at undefined points and asymptotes, never draw across a pole.
        if(!std::isfinite(y0)||!std::isfinite(y1)||!std::isfinite(mid)||y0<lo||y0>hi||y1<lo||y1>hi||mid<lo||mid>hi||
           std::abs(mid-(y0+y1)/2)>(hi-lo)*0.1)continue;
        p.drawLine(QPointF(px(x0),py(y0)),QPointF(px(x1),py(y1)));
    }
    p.restore();return image;
}
