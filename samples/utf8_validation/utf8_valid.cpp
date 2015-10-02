/*
 * This is a simple test application to demonstrate the use of UTF8Reader and give that code a bit of light exercise.
 * It tests four different, well understood malformed UTF8 sequences.
 */
#include <QBuffer>
#include <QByteArray>
#include <QtDebug>
#include <QTimer>
#include <QChar>
#include <QString>
#include <QCoreApplication>

#include "../../src/utf8/utf8_reader.h"

static const char overLongSpace[4]     = {'a', '\xC0', '\xA0', 'b'};
static const char missingMultiByte[4]  = {'a', '\xE2', '\x82', 'b'};
static const char invalidMultiByte[4]  = {'a', '\x80', '\x82', 'b'};

#define FORMAT_CHAR(c) (QStringLiteral("%1").arg(c.unicode(), 4, 16, QLatin1Char('0')))

bool check_char(int count, QChar got, QChar expect)
{
    if(got == expect) {
        qDebug() << "Char validation for count:" << count << "[passed]";
        return true;
    }
    else {
        qDebug() << "Char validation for count:" << count << "[failed]";
        qDebug() << "Expected:" << expect << "code point:" << FORMAT_CHAR(expect) << "got:" << got << "code point:" << FORMAT_CHAR(got);
        return false;
    }
}

int test(const char* data, quint32 len, QChar lead, QByteArray invalid, QChar next, const char* msg)
{
    qDebug() << "\nStarting test case:" << msg;
    QBuffer buf;
    const QByteArray charData(data, len);
    buf.setData(charData);
    if(!buf.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open the buffer QIODevice!";
        return 2;
    }
    UTF8Reader reader;
    int count = 0, result = 0;
    auto sig1 = QObject::connect(&reader, &UTF8Reader::reportBytes, [&result,&invalid](QByteArray seq, qint64 off, qint64 absOff) -> void {
        bool offsetsCheck= off == 1 && absOff == 1;
        bool bytesMatch = seq == invalid;
        if(bytesMatch && offsetsCheck) {
            qDebug() << "Validation of the reported invalid byte sequence [passed]";
        }
        else {
            qDebug() << "Validation of the reported invalid byte sequence [failed]";
            if(!offsetsCheck) {
                qDebug() << "Offsets are off:" << off << "abs:" << absOff << "expected '1' for both.";
                result |= 1;
            }
            if(!bytesMatch) {
                qDebug() << "Sequence is not the expected invalid sequence!";
                result |= 1;
            }
        }
    });
    if(!sig1) {
        qDebug() << "Failed to set up connection !!";
        return 2;
    }
    auto sig2 = QObject::connect(&reader, &UTF8Reader::push, [&count,&result,&lead,&next](QChar c) -> void {
        result = result | (check_char(count, c, count == 0 ? lead : next) ? 0 : 1);
        count ++;
    });
    if(!sig2) {
        qDebug() << "Failed to set up connection !!";
        QObject::disconnect(sig1);
        return 2;
    }
    reader.consume(buf);
    QObject::disconnect(sig1);
    QObject::disconnect(sig2);
    return result;
}


#define INVOKE_TEST(a,msg) (test(a, 4, QLatin1Char(a[0]), QByteArray(&a[1], 2), QLatin1Char(a[3]), msg))

int runTests(void)
{
    return INVOKE_TEST(overLongSpace,"Testing detection of over long encoded ASCII") | 
        INVOKE_TEST(missingMultiByte, "Testing missing trailing byte of multi-byte sequence") |
        INVOKE_TEST(invalidMultiByte, "Testing invalid leading byte of multi-byte sequence");
}

int main(int argc, char** argv) 
{
    QCoreApplication app(argc, argv);
    QTimer::singleShot(0, []() {
        QCoreApplication::exit(runTests());
    });
    return app.exec();
}