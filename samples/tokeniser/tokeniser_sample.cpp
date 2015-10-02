#include "../../src/utf8/utf8_reader.h"
#include "../../src/unit-file/parser/tokeniser.h"
#include <functional>
#include <QBuffer>
#include <QtDebug>
#include <QTimer>
#include <QCoreApplication>

static const QString compiledNewLine(QStringLiteral("\n\n"));

static const QString expComment(QStringLiteral(" This is a simple example"));
static const QString expSection(QStringLiteral("foo"));
static const QString expError(QStringLiteral("trailing-data"));
static const QString expKey(QStringLiteral("bar"));
static const QString expValue(QStringLiteral("baz"));
static const QString NL(QStringLiteral("\n"));

QString createSampleText(void)
{
    return QString(QStringLiteral("#")).append(expComment).append(NL).
        append(QStringLiteral("[")).append(expSection).append(QStringLiteral("]")).append(expError).append(NL).
        append(expKey).append(QStringLiteral("=")).append(expValue);
}

void disconnectAll(QList<QMetaObject::Connection>& connections)
{
    for(auto a: connections) {
        if(a) QObject::disconnect(a);
    }
}

bool verifyConnection(QList<QMetaObject::Connection>& others, const QMetaObject::Connection& conn)
{
    if(conn) {
        others.append(conn);
        return true;
    }
    else {
        qDebug() << "Unable to establish desired signal connection, bailing out!";
        return false;
    }
}

Tokeniser::LineEnding lineEnding(void)
{
    if(compiledNewLine[0] == QLatin1Char('\r')) {
        if(compiledNewLine[1] == QLatin1Char('\n')) {
            return Tokeniser::CRLF;
        }
        else {
            return Tokeniser::CR;
        }
    }
    else {
        return Tokeniser::LF;
    }
}

bool errorMessage(int l, int c, QString v, int el, int ec, QString ev, const char * id)
{
    qDebug() << id << "\t[failed]";
    qDebug() << "Expected value is: "<< ev << "@:" << el << ":" << ec;
    qDebug() << "Received value is: "<< v << "@:" << l << ":" << c;
    return true;
}

bool isWrong(int l, int c,const QString v, int el, int ec, const QString ev, const char * id)
{
    if(l == el && c == ec && v == ev) {
        qDebug() << id << "\t[passed]";
        return false;
    }
    else {
        return errorMessage(l,c,v,el,ec,ev,id);
    }
}

typedef std::function<void(int,int,QString)> TestFunc;

TestFunc createTest(int& result, int line, int column, const QString& exp,const char* id)
{
    TestFunc fn([&result,&exp,line,column,id](int l,int c,QString v) -> void {
        if(isWrong(l, c, v, line, column, exp, id)) {
            result |= 1;
        }
    });
    return fn;
}

TestFunc createRejectFunc(int& result, const char* id)
{
    return TestFunc([&result,id](int l,int c,QString v) -> void {
        qDebug()<< "Didn't expect to receive a"<<id<<"character sequence at:" << l <<":"<< c;
        qDebug() << "Got:"<<v << "size =" <<v.size();
        result |=1;
    });
}

int runTests(void)
{
    int result = 0;
    Tokeniser tk(lineEnding());
    QString sampleText(createSampleText());
    qDebug() << "Will test this sample:";
    qDebug() << sampleText<< "\n";
    QBuffer buf;
    buf.setData(sampleText.toUtf8());
    if(!buf.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open the buffer QIODevice!";
        return 2;
    }
    UTF8Reader reader;
    QList<QMetaObject::Connection> cs;
    auto recoveryErrorHandler=[&result](qint64 at) -> void {
        qDebug() << "Error recovery should not even have been necessary, at:" << at;
        result |=2;
    };
    auto invalidBytesHandler=[&result](QByteArray bytes, qint64, qint64 absOffset) -> void {
        qDebug() << "Found unexpected invalid bytes at:"<< absOffset << "sequence length:" << bytes.size();
        result |=2;
    };
    auto failedHandler=[&result](void) -> void {
        qDebug() << "UTF8 parsing/reading failed.";
        result |=2;
    };
    auto checkComment = createTest(result, 1, 2, expComment, "Comment");
    auto checkSection = createTest(result, 2, 2, expSection, "Section");
    auto checkKey = createTest(result, 3, 1, expKey, "Key");
    auto checkValue = createTest(result, 3, 5, expValue, "Value");
    auto checkError = [&result](int l, int c, QString v, int hint) -> void {
        if(Tokeniser::getError(hint) == Tokeniser::IllegalCharacter && Tokeniser::getToken(hint) == Tokeniser::Space) {
            if(isWrong(l,c,v, 2, 6, expError, "Syntax error")) {
                result |=1;
            }
        }
        else {
            qDebug() << "Syntax error" << "\t[failed]";
            qDebug() << "Not the expected syntax error:"<< QStringLiteral("%1").arg(hint, 2, 16, QLatin1Char('0')) << "at:" << l << ":" << c;
            qDebug() << "In sequence:" << v;
            result |= 1;
        }
    };
    auto rejectSpace = [&result](int l,int c,QString v, int h) -> void {
        qDebug()<< "Didn't expect to receive a 'space' character sequence at:" << l <<":"<< c;
        qDebug() << "Got:"<<v << "size =" <<v.size() << "hint:" << h;
        result |=1;
    };
    auto rejectInclude=createRejectFunc(result, "'include'");
    auto checkDone=[&result]() -> void {
        if(result) {
            qDebug() << "Test failed.";
        }
        else {
            qDebug() << "Test succeeded.";
        }
    };
    
    bool validConn = verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::push, &tk, &Tokeniser::receive)) &&
        verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::pushPair, &tk, &Tokeniser::receivePair)) &&
        verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::done, &tk, &Tokeniser::end)) &&
        verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::doneWithInvalidBytes, &tk, &Tokeniser::end)) &&
        verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::reportBytes, invalidBytesHandler)) &&
        verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::recoveryError, recoveryErrorHandler)) &&
        verifyConnection(cs, QObject::connect(&reader, &UTF8Reader::failed, failedHandler)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::comment, checkComment)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::section, checkSection)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::value, checkValue)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::key, checkKey)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::syntaxError, checkError)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::space, rejectSpace)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::include, rejectInclude)) &&
        verifyConnection(cs, QObject::connect(&tk, &Tokeniser::done, checkDone));
    if(validConn) {
        reader.consume(buf);
    }
    else {
        qDebug() << "Unable to set up signal/slot connections!";
    }
    disconnectAll(cs);
    return result;
}

int main(int argc, char** argv) 
{
    QCoreApplication app(argc, argv);
    QTimer::singleShot(0, []() {
        QCoreApplication::exit(runTests());
    });
    return app.exec();
}