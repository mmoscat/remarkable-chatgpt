// Ink Diary prototype. GPL-3.0-only. See THIRD_PARTY.md.
#include <atomic>
#include <thread>
#include <stdexcept>
#include <QBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTemporaryDir>
#include <QUuid>
#include <QMetaMethod>
#include <QTimer>
#include "inserter.inc"
#include "layout.h"
#include "response.h"
#include "plot.h"
#include "api.h"
#include "diagrams.h"
#include "webanswer.h"
#include "markdown.h"
#include "freshness.h"
#include "letterpaths.h"
#include "pathwriter.inc"

namespace {
std::atomic<bool> busy{false}, cancelled{false};
QMutex stateMutex;
QString message = "ChatGPT";
QPointer<QObject> originController, originView;
QString originPage;
const QString base = "/home/root/.config/ink-diary";
void state(const QString &text) { QMutexLocker lock(&stateMutex); message = text; }
void require(bool valid, const char *why) { if (!valid) throw std::runtime_error(why); }

QImage capture(const QString &path) {
    QFile::remove(path);
    QByteArray arg = (path + ",0,0").toUtf8();
    char *result = rm_shot$screenshotHandler(arg.data());
    bool started = result && QByteArray(result) == "success";
    free(result);
    require(started, "Screenshot failed to start");
    // rm-shot writes asynchronously. Decode rather than accepting file existence.
    for (int i=0; i<100; ++i) {
        require(diaryGuard(), "Cancelled or page changed");
        QThread::msleep(100);
        QImage image(path);
        if (!image.isNull()) return image;
    }
    throw std::runtime_error("Screenshot timed out");
}

QString answer(const QImage &image, const QJsonObject &config, int /*lines*/) {
    QString key = config.value("api_key").toString().trimmed();
    QString model = config.value("model").toString().trimmed();
    require(!key.isEmpty() && !model.isEmpty(), "Set api_key and model in config.json");
    QByteArray png;
    QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
    QString prompt = QString("This is a handwritten notebook page. Answer the latest unanswered question. "
        "Earlier writing may include prior answers. If handwriting is unclear, ask briefly for clarification. "
        "Reply in the question's language using GitHub Markdown where useful: headings, tables, lists, emphasis, quotes and fenced code. "
        "For checklists, emit one Markdown task-list item per line (- [ ]). For unlabeled checkboxes, emit one empty task-list item per line with no visible label; never put multiple checkbox symbols on a single line. Your answer is sent directly to a GitHub Markdown renderer, not shown as Markdown source. Emit tables, headings, lists and templates as actual Markdown outside code fences. Never wrap the entire answer or a Markdown table in a code block merely because the user asks for Markdown. Use code fences only for actual programming code or when the user explicitly asks to see raw Markdown source or syntax. "
        "Keep answers concise, usually under 180 words. Keep tables narrow (at most five columns). "
        "Use plain mathematical notation, not LaTeX. Do not use HTML or external image URLs in Markdown. "
        "Do not repeat the question. Always answer by calling the ink_diary_answer tool with fields matching its schema. "
        "Use kind=text for ordinary answers; fill text with your answer, command must transcribe the latest question including context needed to understand follow-ups; expression empty, xmin=-5,xmax=5. "
        "VISUAL ROUTING: Generating pictures, illustrations, comic strips or manga panels is not available. If asked to draw such artwork, use kind=text, say briefly that artwork generation is unavailable, and offer a flowchart, sequence diagram, Venn diagram or written description instead. Fiction set today does not require search. "
        "DIAGRAM ROUTING: Requests to create flowcharts, Mermaid diagrams, or sequence diagrams use kind=mermaid. "
        "text is bare Mermaid source, no code fences. Supported subset ONLY: flowchart TD or graph TD, "
        "declare every node on a separate line as A[\"Label\"], B(\"Label\"), or C{\"Decision\"}; "
        "then edges on separate lines A --> B or A -->|Yes| B. No inline node declarations on edges, chained edges, subgraphs, styling, HTML, links or directives. "
        "Use at most 12 nodes and 24 edges; concise labels (decisions at most 15 characters), edge labels at most 15 characters. "
        "Sequence diagrams: start sequenceDiagram, declare 2-5 participants using participant A as Name on separate lines, "
        "then at most 20 messages A->>B: Message or B-->>A: Reply. No loops/notes/activation syntax. Keep messages short. "
        "For unsupported Mermaid types (class, ER, Gantt, state, etc), explain supported types with kind=text; do not silently substitute. "
        "For Venn diagrams use kind=venn with text containing serialized JSON: {\"sets\":[\"Set A\",\"Set B\"],\"regions\":[{\"mask\":1,\"text\":\"A only\"},{\"mask\":2,\"text\":\"B only\"},{\"mask\":3,\"text\":\"Both\"}]}. "
        "Support 2 or 3 sets. Bitmask 1=A,2=B,4=C; include every exclusive region mask 1..3 or 1..7 exactly once, empty text for blank regions. "
        "Do not claim circle areas represent counts. Region text appears in a numbered legend so it stays readable. "
        "For all visual kinds command transcribes the request, expression is empty, xmin=-5,xmax=5; use the empty template. "
        "TEMPLATE ROUTING: When asked to create a checklist or a single table/template (excluding calendars), use kind=checklist or kind=table. "
        "Populate template with title (optional empty string), count (exact requested number of items or body rows), labels, columns, rows, and cell_lines. "
        "For a blank checklist labels=[]; the app generates count empty boxes. For a labeled checklist labels must contain exactly count strings. "
        "For a table columns contains column names, rows contains exactly count arrays of cells; for blank table body use rows=[]. "
        "For tables, cell_lines is the requested number of handwriting lines per cell (1=compact, 3=roomy); it controls cell HEIGHT, not the number of table rows. "
        "CALENDAR ROUTING: For a monthly calendar use kind=calendar and calendar={year:requested year,month:1..12,week_start:7 for Sunday or 1 for Monday}. "
        "Default to Sunday unless requested otherwise. Resolve relative months from the supplied local date; ask for clarification when month/year cannot be determined. "
        "Never compute the weekday, dates, rows or columns yourself: the app calculates the entire Gregorian calendar locally. "
        "Set template.cell_lines=3 unless the user requests a different cell height; keep the other template fields empty/count=0. "
        "For calendar text and expression are empty, xmin=-5,xmax=5 and command transcribes the request. "
        "For non-calendar kinds always set calendar={year:2000,month:1,week_start:7}. Ordinary tables default cell_lines=1. "
        "Do not insert HTML br tags or fake blank rows/newlines to create writing space; use cell_lines. "
        "Do not put Markdown syntax in template labels/cells. For a pros/cons template use columns=[Pros,Cons] and blank rows, "
        "choosing five rows if no number is given. Checklist count likewise defaults to ten if unspecified. "
        "For template kinds text and expression are empty, xmin=-5,xmax=5, and command transcribes the latest request. "
        "For other non-calendar kinds always use template={title:empty string,count:0,labels:[],columns:[],rows:[],cell_lines:1}. "
        "Current-fact requests still require kind=search even when the requested answer includes a table; use Markdown for mixed explanatory answers. "
        "Only when the latest unanswered handwritten request starts with the word plot followed by an equation, use kind=plot. "
        "Transcribe that request in command. Support a single explicit real function y=f(x) (or just f(x)). "
        "Normalize the right-hand side into expression using only x, decimal numbers, pi,e,+,-,*,/,^,parentheses, "
        "sin,cos,tan,sqrt,abs,exp,ln,log. Use explicit multiplication (2*x), ^ for powers, radians for trig, natural log. "
        "Use the written x range if supplied; otherwise xmin=-5,xmax=5. For plots text is empty. "
        "For unclear handwriting or unsupported equations (implicit, parametric, multiple functions, other variables), "
        "use kind=text with a brief clarification; do not invent or change the equation. "
        "Ignore old plot commands that already have a graph beneath them. "
        "WEB SEARCH ROUTING: If the latest question starts with Search (case-insensitive), or explicitly asks to look up, "
        "search, verify online, or find sources, ALWAYS return kind=search. Also use kind=search automatically for "
        "current/recent information, news, weather, prices, schedules, current officeholders or other facts requiring "
        "up-to-date verification. In search mode, text must be the user's question rewritten as a standalone query "
        "including necessary notebook context and the user's language, NOT an answer. command transcribes the request, "
        "expression is empty, xmin=-5,xmax=5. Do not invent missing locations or dates; ask clarification with kind=text. "
        "Examples: Who is the current president? -> search; Who is the US president today? -> search; "
        "What happened this week? -> search. Never answer these from training data. "
        "Ordinary timeless questions and plot commands need no search.");
    prompt.prepend(diaryCurrentContext(QDateTime::currentDateTimeUtc(),config.value("timezone").toString("America/New_York")));
    QJsonObject payload{{"model",model},{"max_tokens",2600},{"system",prompt},
        {"tools",QJsonArray{diaryAnswerTool()}},
        {"tool_choice",QJsonObject{{"type","tool"},{"name","ink_diary_answer"}}},
        {"messages",QJsonArray{QJsonObject{{"role","user"},{"content",QJsonArray{
            QJsonObject{{"type","image"},{"source",QJsonObject{{"type","base64"},{"media_type","image/png"},
                {"data",QString::fromLatin1(png.toBase64())}}}},
            QJsonObject{{"type","text"},{"text","Read the latest unanswered question on this notebook page."}}
        }}}}}};
    return diaryToolJson(diaryApiRequest(payload,key,diaryGuard));
}

void work() {
    try {
        QTemporaryDir temp("/tmp/ink-diary-XXXXXX");
        require(temp.isValid(), "Cannot create private temporary directory");
        QFile file(base+"/config.json");
        require(file.open(QIODevice::ReadOnly), "Create ink-diary/config.json first");
        QJsonObject config=QJsonDocument::fromJson(file.readAll()).object();
        require(!config.isEmpty(), "Invalid config.json");
        state("Reading page...");
        // Let the button switch to its stable busy appearance before capture.
        QThread::msleep(800);
        auto screen=capture(temp.path()+"/question.png");
        require(screen.width()==1404 && screen.height()==1872,
                "First version requires rM2 portrait (1404 x 1872)");
        QSize canvas;
        runOnGuiSync([&] {canvas=activeCanvasSize();});
        require(canvas==screen.size(), "Screen/input coordinates differ; refusing to draw");
        const QRect area=diaryCaptureArea();
        QImage question=screen.copy(area);
        int last=lastInkRow(question);
        require(last>=0, "Write a question on a blank page first");
        int top=area.y()+last+45;
        // Native scene images can extend below the current viewport. Render at
        // a fixed readable size, independent of empty space on screen.
        int width=area.width(), available=1582;
        state("Thinking...");
        bool demo=config.value("demo_mode").toBool(false);
        DiaryAnswer resultAnswer;
        if(demo) resultAnswer.text="This is a handwriting test. Live answers will work after you configure an Anthropic API key.";
        else resultAnswer=decodeDiaryAnswer(answer(question,config,qMin(9,available/60)));
        if(!demo)diaryRequireFreshSearch(resultAnswer);
        DiaryWebAnswer web;
        if(resultAnswer.search) {
            state("Searching the web...");
            web=parseDiarySearch(diaryApiRequest(diarySearchPayload(resultAnswer.text,config.value("model").toString(),config.value("timezone").toString("America/New_York")),
                config.value("api_key").toString(),diaryGuard));
            web.checkedAt=diaryLocalClock(QDateTime::currentDateTimeUtc(),config.value("timezone").toString("America/New_York"));
            resultAnswer.text=web.text;
        }
        QString text=resultAnswer.text;
        QImage ink; QString renderError, renderedHtml;
        require(!resultAnswer.image,"Artwork generation is not available with Claude; try a diagram");
        runOnGuiSync([&] {
            if(resultAnswer.image)return;
            if(!resultAnswer.diagram.isEmpty()) {
                try {ink=renderDiaryDiagram(resultAnswer.diagram,text,width);}
                catch(const std::exception &error){renderError=QString::fromUtf8(error.what());}
                return;
            }
            if(resultAnswer.plot) {
                try {ink=renderPlot(resultAnswer,width,available);}
                catch(const std::exception &error){renderError=QString::fromUtf8(error.what());}
                return;
            }
            static int fontId=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
            static int monoId=QFontDatabase::addApplicationFont(base+"/DejaVuSansMono.ttf");
            if(monoId<0) {renderError="Code font missing; reinstall Ink Diary";return;}
            if(fontId<0) {renderError="Caveat font missing";return;}
            try {
                QStringList sourceUrls;for(const auto &source:web.sources)sourceUrls.append(source.url);
                auto rendered=renderDiaryMarkdown(text,QFontDatabase::applicationFontFamilies(fontId).first(),width,
                    resultAnswer.search?diarySourcesText(web):QString(),sourceUrls,resultAnswer.cellLines);
                ink=rendered.image;renderedHtml=rendered.html;
            } catch(const std::exception &error) {renderError=QString::fromUtf8(error.what());}
        });
        if(!renderError.isEmpty()) throw std::runtime_error(renderError.toStdString());
        state("Checking page...");
        auto current=capture(temp.path()+"/check.png");
        require(current.copy(area)==question, "Page changed while thinking; tap ChatGPT again");
        require(diaryGuard(), "Cancelled or page changed");
        const QString answerDir="/home/root/.local/share/ink-diary/answers";
        require(QDir().mkpath(answerDir), "Cannot create answer image directory");
        QFile::setPermissions(answerDir,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        QString imagePath=answerDir+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".png";
        require(ink.save(imagePath), "Cannot save answer image");
        QFile::setPermissions(imagePath,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        QString htmlPath=imagePath+".html";
        if(!renderedHtml.isEmpty()) {
            QFile html(htmlPath);
            require(html.open(QIODevice::WriteOnly),"Cannot save source links");
            html.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            QByteArray bytes=renderedHtml.toUtf8();
            require(html.write(bytes)==bytes.size(),"Cannot save source links");
        }
        QString sourcePath=imagePath+(resultAnswer.image || !resultAnswer.diagram.isEmpty() ? ".source.txt" : ".md");
        if(!resultAnswer.plot) {
            QFile raw(sourcePath);
            require(raw.open(QIODevice::WriteOnly),"Cannot save answer Markdown");
            raw.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            auto bytes=text.toUtf8();require(raw.write(bytes)==bytes.size(),"Cannot save answer Markdown");
        }
        require(diaryGuard(), "Cancelled or page changed");
        state("Inserting image...");
        auto result=insertSceneImage(QJsonObject{{"path",imagePath},{"coordinateSpace","screen"},
            {"x",area.x()+width/2.0},{"y",top+ink.height()/2.0},{"appendBelowContent",true}});
        if(!result.value("ok").toBool()) {
            QFile::remove(imagePath);
            QFile::remove(htmlPath);
            QFile::remove(sourcePath);
            throw std::runtime_error(result.value("message").toString().toStdString());
        }
        state(resultAnswer.image ? "Grayscale image added; scroll down if needed" : !resultAnswer.diagram.isEmpty() ? "Diagram added; scroll down if needed" : resultAnswer.plot ? "Plot added; scroll down if needed" : resultAnswer.search ? (web.sources.isEmpty() ? "Web answer added; source links unavailable" : "Web answer + sources added") : "Answer added; scroll down if needed");

    } catch(const std::exception &error) { state(QString::fromUtf8(error.what())); }
    busy=false;
}
}

static bool diaryGuard() {
    if(cancelled) return false;
    bool valid=false;
    runOnGuiSync([&] {
        auto now=findActivePage();
        valid=originController && originView && now.controller==originController.data()
            && now.documentView==originView.data() && now.pageId==originPage;
    });
    if(!valid) cancelled=true;
    return valid;
}
extern "C" char *diary_start(const char *) {
    if(busy.exchange(true)) return response(QJsonObject{{"ok",false},{"message","Already busy"}});
    cancelled=false;
    bool valid=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        originController=context.controller; originView=context.documentView; originPage=context.pageId;
        valid=originController && originView && !originPage.isEmpty();
    });
    if(!valid) {busy=false; state("Open a notebook page first"); return response(QJsonObject{{"ok",false}});}
    state("Starting...");
    std::thread(work).detach();
    return response(QJsonObject{{"ok",true}});
}
extern "C" char *diary_cancel(const char *) { cancelled=true; return response(QJsonObject{{"ok",true}}); }
extern "C" char *diary_status(const char *) {
    QFile config(base+"/config.json");
    bool demo=config.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(config.readAll()).object().value("demo_mode").toBool();
    QMutexLocker lock(&stateMutex);
    return response(QJsonObject{{"busy",busy.load()},{"message",message=="ChatGPT" && demo ? "ChatGPT (demo)" : message}});
}
extern "C" char _xovi_shouldLoad() {
    QFile version("/usr/share/remarkable/update.conf");
    if (!version.open(QIODevice::ReadOnly) || !version.readAll().split('\n').contains("REMARKABLE_RELEASE_VERSION=3.27.3.0")) return false;
    return dlsym(RTLD_DEFAULT,"_Z21qRegisterResourceDataiPKhS0_S0_")!=nullptr;
}
extern "C" char *diary_probe(const char *arg) {
    if(arg && QByteArray(arg)=="checklist") {
        QJsonObject check;
        runOnGuiSync([&] {
            try {
                int id=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
                QString text;for(int i=0;i<10;++i)text+="- [ ]\n";
                auto rendered=renderDiaryMarkdown(text,QFontDatabase::applicationFontFamilies(id).first(),1174);
                check.insert("checkboxes",rendered.html.count("class=\"unchecked\""));
                check.insert("height",rendered.image.height());
            } catch(const std::exception &e){check.insert("error",QString::fromUtf8(e.what()));}
        });
        return response(check);
    }
    auto result=capabilities();
    bool imageSupport=false,mapping=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        if(!context.controller)return;
        for(int i=0;i<context.controller->metaObject()->methodCount();++i)
            if(context.controller->metaObject()->method(i).name()=="insertImageFileAsSceneItem")imageSupport=true;
        for(auto object:runtimeObjects()) {
            if(!qobject_cast<QQuickItem *>(object) || !isVisibleObject(object) || controllerProperty(object,"controller")!=context.controller)continue;
            for(int i=0;i<object->metaObject()->methodCount();++i)
                if(object->metaObject()->method(i).name()=="viewToScene")mapping=true;
        }
    });
    result.insert("nativeImageInsertAvailable",imageSupport);result.insert("screenMappingAvailable",mapping);
    return response(result);
}
