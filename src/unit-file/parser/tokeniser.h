#ifndef SD_UIKIT_UNITFILE_TOKENISER
#define SD_UIKIT_UNITFILE_TOKENISER

#include <QChar>
#include <QObject>
#include <QString>


class TokeniserPrivate;

class Tokeniser: public QObject {
    Q_OBJECT
    Q_ENUMS(LineEnding)
    Q_ENUMS(SyntaxError)
    Q_ENUMS(TokenType)
public:
    // Enum values make it easy to test if either \r or \n matter. I.e. bitmasks.
    enum LineEnding {
        CR = 1, /* carriage return aka '\r' */
        LF = 2, /* linefeed (newline) aka '\n' */
        CRLF = 3, /* CR + LF aka "\r\n" */
        Both = 7 /* CR or LF aka recover from broken stuff */
    };
    enum SyntaxError {
        NotASyntaxError  = 0,
        MissingIdentifier= 1,
        UnterminatedItem = 2,
        IllegalCharacter = 3
    };
    enum TokenType {
        Space = 0,
        Key = 4, 
        Value = 8,
        Section = 12
    };
    static constexpr inline enum Tokeniser::SyntaxError getError(int hintCode) { return (Tokeniser::SyntaxError) (hintCode & 0x3); }
    static constexpr inline enum Tokeniser::TokenType getToken(int hintCode) { return (Tokeniser::TokenType) (hintCode & 0xC); }
    Tokeniser(const LineEnding& lineEnding, QObject * parent = 0);
    virtual ~Tokeniser();
Q_SIGNALS:
    void done(void);
    void key(int line, int column, QString name);
    void value(int line, int column, QString value);
    void section(int line, int column, QString name);
    void space(int line, int column, QString sequence, int hint);
    void comment(int line, int column, QString comment);
    void include(int line, int column, QString import);
    void syntaxError(int line, int column, QString token, int hint);
public Q_SLOTS:
    void receive(QChar c);
    void receivePair(QChar fst, QChar snd);
    void end(void);
private:
    Q_DISABLE_COPY(Tokeniser)

    Q_DECLARE_PRIVATE(Tokeniser)
    TokeniserPrivate *const d_ptr;
};

#endif