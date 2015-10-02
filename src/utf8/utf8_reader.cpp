
#include "utf8_reader.h"
#include <QTextStream>
#include <QString>
#include <QtDebug>

#define COUNT_UTF8_BYTE(cP, l, sz) ((cP) <= (l) ? (sz) : (sz + 1))

static inline qint64 utf8SequenceLength(uint codePoint)
{
    //invalid?
    if(codePoint == 0xFFFD || codePoint > 0x10FFFF) {
        return -1;
    }
    else {
        return COUNT_UTF8_BYTE(codePoint, 0xFFFF, COUNT_UTF8_BYTE(codePoint, 0x7FF, COUNT_UTF8_BYTE(codePoint, 0x7F, 1)));
    }
}

#undef COUNT_UTF8_BYTE

typedef enum { Clear = 0, Hi, Low, SurrogatePair } UTFState;

UTF8Reader::UTF8Reader(QObject * parent) : QObject(parent) {}

void UTF8Reader::consume(QIODevice & input) { consume(&input); }

void UTF8Reader::consume(QIODevice * input)
{
    qint64 count = 0, offset = input->pos(), size, location;
    UTFState utfState = Clear, oldState = Clear;
    QChar chr, hi, lo;
//     /*
//      * There is no nice way to check for errors at the QIODevice abstraction level.
//      * We use a proxy (the *existence* of a value other than a known-good one) as a way to poll for I/O errors.
//      * Define the known-good error string here and set it.
//      */
//     QString errorOK(QStringLiteral(""));
//     input->setErrorString(errorOK);
    QTextStream str(input);
    str.setCodec("UTF-8");
    str.setAutoDetectUnicode(false);
    bool fail = false, hasBadBytes = false;
    char byte;
    QByteArray badBytes;
    
    while(!fail && !str.atEnd()) {
        /*
         * In order to be able to report the location of 'bad bytes' accurately, the 
         * 'count' of bytes consumed is only updated when some valid data is pushed out.
         * Location is used as an updated version of 'count' with the 'bad bytes' currently buffered accounted for.
         */
        location = offset + count + badBytes.size();
//         if(input->errorString() != errorOK || str.status() != QTextStream::Ok) {
//             emit readError(location);
//             fail = true;
//             break;
//         }
        
        oldState = utfState;
        str >> chr;
        if(chr.isHighSurrogate()) {
            if((utfState & Hi) == 0) {
                utfState = utfState == Clear ? Hi : SurrogatePair;
                hi = chr;
            }
        }
        if(chr.isLowSurrogate()) {
            if((utfState & Low) == 0) {
                utfState = utfState == Clear ? Low : SurrogatePair;
                lo = chr;
            }
        }
        if(utfState == SurrogatePair) {
            size = utf8SequenceLength(QChar::surrogateToUcs4(hi,lo));
            if(size > 0) {
                if(badBytes.size() > 0) {
                    emit reportBytes(badBytes, count, offset + count);
                    count += badBytes.size();
                    badBytes.clear();
                }
                if(oldState == Hi) {
                    qDebug() << "Pushing hi-lo surrogate pair";
                    emit pushPair(hi, lo);
                }
                else {
                    qDebug() << "Pushing lo-hi surrogate pair";
                    emit pushPair(lo, hi);
                }
                utfState = Clear;
                count += size;
                continue;
            }
        }
        else {
            if(utfState == Clear) {
                size = utf8SequenceLength(chr.unicode());
                if(size > 0) {
                    if(badBytes.size() > 0) {
                        emit reportBytes(badBytes, count, offset + count);
                        count += badBytes.size();
                        badBytes.clear();
                    }
                    emit push(chr);
                    count += size;
                    continue;
                }
            }
            else {
                /*
                 * We have only a single half of a surrogate pair.
                 * The next character is (presumably) the other half.
                 * Avoid jumping into error recovery (this situation is not an error).
                 * 
                 * If oldState != Clear, we have a imbalanced surrogate pair sequence (lo-lo or hi-hi) which is an error.
                 */
                if(oldState == Clear) continue;
            }
        }
        
        /*
         * If we get here, the UTF8 sequence contains a malformed section.
         * Attempt to recover: 
         *  - reset utf & I/O error state,
         *  - seek to last known position, 
         *  - read 1 byte and add it to the badBytes
         *  - resync QTextStream with QIODevice to the correct updated position to resume from
         */
        utfState = Clear;
//         str.setStatus(QTextStream::Ok);
//         input->setErrorString(errorOK);
        
        if(input->seek(location)) {
            if(input->getChar(&byte)) {
                badBytes.append(byte);
                hasBadBytes = true;
                // synchronise text stream with underlying QIODevice using an explicit seek.
                if(!str.seek(location + 1)) {
                    emit recoveryError(location + 1);
                    fail = true;
                }
            }
            else {
                emit recoveryError(location);
                fail = true;
            }
        }
        else {
            emit recoveryError(location);
            fail = true;
        }
    }
    // check if the stream ended with an invalid byte sequence trailing the UTF8 data & report if true.
    if(badBytes.size() > 1) {
        emit reportBytes(badBytes, count, offset + count);
    }
    if(fail) {
        emit failed();
    }
    else {
        if(hasBadBytes) {
            emit doneWithInvalidBytes();
        }
        else {
            emit done();
        }
    }
}