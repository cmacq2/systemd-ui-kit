#ifndef SD_UIKIT_UTF8_READER_H
#define SD_UIKIT_UTF8_READER_H

#include <QByteArray>
#include <QChar>
#include <QIODevice>
#include <QObject>

/**
 * \brief Provides a validating layer on top of a QIODevice for reading UTF8 encoded text.
 * This class addresses the problem that invalid byte sequences would otherwise be silently converted to sequences of the Unicode code point 0xFFFD or stripped.
 * It is a validating alternative from using e.g. QTextStream directly.
 * \note Qt uses a UTF-16 based scheme for representing characters (QChar) instead of UTF-8. This means that the UTF8Reader may report characters either as 
 * a single QChar or as a surrogate pair of QChar.
 */
class UTF8Reader: public QObject
{
    Q_OBJECT
public:
    UTF8Reader(QObject * parent = 0);
    /**
     * \brief reads UTF-8 encoded text from the given input stream until it is exhausted.
     * This is a convenience flavour of #consume(QIODevice*)
     */
    void consume(QIODevice & input);
    /**
     * \brief reads UTF-8 encoded text from the given input stream until it is exhausted.
     */
    void consume(QIODevice * input);
Q_SIGNALS:
    /**
     * \brief emitted when a valid character is found.
     * \param c the character which was found. It is guaranteed not to be part of a surrogate pair.
     */
    void push(QChar c);
    /**
     * \brief emitted when a valid character is found, the character takes the form of a surrogate pair.
     * \param fst the first half of the surrogate pair which was found
     * \param snd the second half of the surrogate pair which was found
     * 
     * \note This is the common case. If you need to parse things, you can probably simply append the data to some string/buffer.
     */
    void pushPair(QChar fst, QChar snd);
    /**
     * \brief emitted when a invalid byte sequence (malformed UTF-8) is found.
     * \param relOffset the offset at which the invalid data was found. This offset is relative to the position of the QIODevice when it was passed to #consume(QIODevice*).
     * \param absOffset the absolute offset in the QIODevice at which the invalid data was found.
     */
    void reportBytes(QByteArray invalidSequence, qint64 relOffset, qint64 absOffset);
    /**
     * \brief emitted when input has been consumed successfully and no malformed UTF-8 was encountered.
     */
    void done(void);
    /**
     * \brief emitted when input has been consumed successfully but invalid byte sequences were encountered (malformed UTF-8).
     */
    void doneWithInvalidBytes(void);
    /**
     * \brief emitted when UTF8Reader failed to consume the input successfully. Input may be partially consumed.
     */
    void failed(void);
    /**
     * \brief emitted when some I/O operation fails (during error recovery, as the result of malformed UTF-8 sequences).
     */
    void recoveryError(qint64 at);
};

#endif