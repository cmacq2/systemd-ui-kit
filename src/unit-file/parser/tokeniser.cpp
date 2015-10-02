#include "tokeniser.h"


class LineCounter {
public:
    LineCounter(const Tokeniser::LineEnding& type) : m_nlType(type), m_prev(QLatin1Char('\0')), m_line(1), m_column(0) {}
    int line(void) const { return m_line; }
    int column(void) const { return m_column; }
    QChar previous(void) const { return m_prev; }
    
    bool push(QChar c)
    {
        bool nlFound = (c == QLatin1Char('\r') && m_nlType & Tokeniser::CR && m_nlType != Tokeniser::CRLF) || 
                        (c == QLatin1Char('\n') && m_nlType & Tokeniser::LF && (m_nlType != Tokeniser::CRLF || m_prev == QLatin1Char('\r')));
        
        if(nlFound) {
            m_prev = QLatin1Char('\0');
            m_column = 0;
            m_line ++;
        }
        else {
            m_prev = c;
            m_column++;
        }
        return nlFound;
    }
    
    bool pushPair(QChar, QChar)
    {
        m_column++;
        m_prev = QLatin1Char('\0');
        return false;
    }
    
    bool retraceCR(void)
    {
        if(m_prev == QLatin1Char('\r')) {
            m_column--;
            return true;
        }
        else {
            return false;
        }
    }
    
    void retraceFixCount(void)
    {
        m_column++;
    }
    
private:
    const Tokeniser::LineEnding m_nlType;
    QChar m_prev;
    int m_line, m_column;
};

typedef enum TokenClass { 
    Section, Key, Value, Space, Comment, Syntax, Error
} TokenClass;

class TokenClassifier 
{
public:
    TokenClassifier() { resetToNewLine(); }
    TokenClass bias(void) const { return m_bias; }
    bool startChar(void) const { return m_startChar; }
    
    void resetToNewLine(void)
    {
        m_bias = Syntax;
        m_startChar = false;
    }
    
    void resetToValue(void)
    { 
        m_bias = Value;
    }
    
    TokenClass type(QChar c)
    {
        TokenClass result = Error;
        if(c == QLatin1Char('[')) {
            if(m_bias == Syntax) {
                m_bias = Section;
                m_startChar = false;
                result = Syntax;
            }
        }
        else if(c == QLatin1Char(']')) {
            if(m_bias == Section && m_startChar) {
                m_bias = Space;
                result = Syntax;
            }
        }
        else if(c == QLatin1Char('=')) {
            if(m_bias == Key) {
                m_startChar = false;
                m_bias = Value;
                result = Syntax;
            }
        }
        else if(c == QLatin1Char(';') || c == QLatin1Char('#')) {
            if(m_bias == Syntax) {
                m_startChar = true;
                m_bias = Comment;
                result = Syntax;
            }
        }
        
        // none of the above if-clauses fired
        if(result != Syntax) {
            result = validateAgainstType(c);
        }
        
        return result;
    }
    
    TokenClass type(QChar fst, QChar snd) 
    {
        if(fst.isSurrogate() && snd.isSurrogate()) {
            switch(m_bias) {
                case Comment:
                case Value:
                    m_startChar = true;
                    return m_bias;
                default:
                    return Error;
            }
        }
        else {
            return Error;
        }
    }
private:
    inline bool isSpace(QChar c) 
    {
        return c == QLatin1Char(' ') || c == QLatin1Char('\t');
    }
    /*
     * According to the systemd unit file man page "The syntax is inspired by XDG Desktop Entry Specification .desktop files"
     * According to the XDG Desktop file spec (v 1.1), "Only the characters A-Za-z0-9- may be used in key names."
     */
    inline bool isKeyChar(QChar c)
    {
        return (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) || 
               (c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
               (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
               c == QLatin1Char('-');
    }
    /*
     * According to the systemd unit file man page "The syntax is inspired by XDG Desktop Entry Specification .desktop files"
     * According to the XDG Desktop file spec (v 1.1), a section name may contain "Any ASCII char except [ and ] and control characters".
     */
    inline bool isSectionChar(QChar c)
    {
        return c >= QLatin1Char(' ') && c <= QLatin1Char('~') && c != QLatin1Char('[') && c != QLatin1Char(']');
    }
    
    TokenClass validateAgainstType(QChar c) 
    {
        if(isSpace(c)) {
            switch(m_bias) {
                case Section:
                    m_startChar = true;
                    return isSectionChar(c) ? m_bias: Error;
                case Value:
                    /*
                     * Eat up leading whitespace for values. 
                     * m_startChar is set to true once a non-whitespace character is found in the Value.
                     */
                    return m_startChar ? m_bias: Space;
                // NOTE fall through logic used here.
                case Syntax:
                    m_bias = Space; // leading whitespace on lines is only allowed if all subsequent stuff is such whitespace.
                case Key:
                    m_startChar = true; // m_startChar is used to mark-end-of-key i.e. space is considered trailing.
                default:
                    return m_bias;
            }
        }
        else {
            switch(m_bias) {
                case Syntax:
                    /*
                     * Assume the start of alpha numeric content indicates a key.
                     */
                    if(isKeyChar(c)) {
                        m_startChar = false; // m_startChar is used for whitespace related purposes now, reset to false.
                        m_bias = Key;
                        return m_bias;
                    }
                    else {
                        return Error;
                    }
                case Space:
                    return Error; // we're in a situation in which only (trailing) whitespace is allowed
                case Comment:
                    return m_bias;
                case Value:
                    m_startChar = true; // we have found a non-whitespace character in the Value, use m_startChar to mark the occasion.
                    return m_bias; // may need to filter out control chars?
                case Key:
                    /*
                     * m_startChar is used to mark end-of-key when trailing whitespace is encountered.
                     * Further trailing content would therefore indicate an error, might as well report *that* content as the error instead of the whitespace sequence.
                     */
                    return !isKeyChar(c) || m_startChar ? Error: m_bias;
                case Section:
                    m_startChar = true;
                    return isSectionChar(c) ? m_bias: Error;
                default:
                    return Error;
            }
        }
    }
private:
    TokenClass m_bias;
    bool m_startChar;
};

class TokeniserPrivate {
public:
    TokeniserPrivate(const Tokeniser::LineEnding& nl, Tokeniser * q) : q_ptr(q), m_type(Syntax), m_counter(nl) { mark(); }
    
    void push(QChar c)
    {
        bool retraceCR = m_counter.retraceCR();
        if(m_counter.push(c)) {
            newLineDecision();
        }
        else {
            retrace(retraceCR);
            if(c != QLatin1Char('\r')) {
                update(m_cls.type(c));
                m_token.append(c);
            }
        }
    }
    
    void pushPair(QChar fst, QChar snd)
    {
        bool retraceCR = m_counter.retraceCR();
        if(m_counter.pushPair(fst, snd)) {
            newLineDecision();
        }
        else {
            retrace(retraceCR);
            update(m_cls.type(fst, snd));
            m_token.append(fst);
            m_token.append(snd);
        }
    }
    
    void finish(void)
    {
        Q_Q(Tokeniser);
        retrace(m_counter.retraceCR());
        flush();
        emit q->done();
    }
    
private:
    
    Tokeniser * const q_ptr;
    Q_DECLARE_PUBLIC(Tokeniser)
    
    void reportToken(TokenClass categoryHint, TokenClass type, QString token, int line, int column)
    {
        Q_Q(Tokeniser);
        int hint = 0;
        switch(type) {
            case Error:
                hint = hintCode(Tokeniser::IllegalCharacter, tokenType(categoryHint));
                emit q->syntaxError(line, column, token, hint);
                break;
            case Syntax:
                if(token == QStringLiteral("[")) {
                    hint = hintCode(Tokeniser::MissingIdentifier, Tokeniser::Section);
                }
                else {
                    hint = hintCode(Tokeniser::UnterminatedItem, tokenType(m_type));
                }
                emit q->syntaxError(line, column, token, hint);
                break;
            case Space:
                hint = hintCode(Tokeniser::NotASyntaxError, tokenType(categoryHint));
                emit q->space(line, column, token, hint);
                break;
            case Key:
                emit q->key(line, column, token);
                break;
            case Value:
                emit q->value(line, column, token);
                break;
            case Section:
                emit q->section(line, column, token);
                break;
            case Comment:
                emit q->comment(line, column, token);
                break;
            default:
                break;
        }
    }
    
    inline int hintCode(Tokeniser::SyntaxError err, Tokeniser::TokenType type)
    {
        return ((int) err) | ((int) type);
    }
    
    Tokeniser::TokenType tokenType(TokenClass categoryHint)
    {
        switch(categoryHint) {
            case Syntax:
            case Key:
                return Tokeniser::Key;
            case Section:
                return Tokeniser::Section;
            case Value:
                return Tokeniser::Value;
            default:
                return Tokeniser::Space;
        }
    }
    
    void flush(void)
    {
        switch(m_type)
        {
            case Section:
                report();
                reportToken(Syntax, Syntax, QStringLiteral("]"), m_tokenLine, m_tokenColumn + m_token.size());
            case Key:
                report();
                reportToken(Syntax, Syntax, QStringLiteral("="), m_tokenLine, m_tokenColumn + m_token.size());
                break;
            case Space:
            case Error:
            case Value:
            case Comment:
                report();
                break;
            case Syntax:
                TokenClass bias= m_cls.bias();
                switch(bias) {
                    case Section: // just a single '['
                        reportToken(Syntax, Syntax, QStringLiteral("["), m_tokenLine, 1);
                        break;
                    case Comment:
                    case Value:
                        reportToken(bias, bias, m_token, m_tokenLine, m_tokenColumn); // synthesise 'empty' value/comment token
                        break;
                    default:
                        break;
                }
                break;
        }
    }
    
    void report(void)
    {
        if(m_type != Syntax) {
            TokenClass bias = m_cls.bias();
            reportToken(bias == Syntax ? m_type : bias, m_type, m_token, m_tokenLine, m_tokenColumn);
        }
    }
    
    void mark(void)
    {
       m_tokenLine = m_counter.line();
       m_tokenColumn = m_counter.column();
       m_token = QStringLiteral("");
    }
    
    void update(TokenClass t)
    {
        if(t != m_type) {
            report();
            mark();
            m_type = t;
        }
    }
    
    void retrace(bool retraceCR)
    {
        if(retraceCR) {
            update(m_cls.type(QLatin1Char('\r')));
            m_token.append(QLatin1Char('\r'));
            m_counter.retraceFixCount();
        }
    }
    
    void newLineDecision(void)
    {
        if(m_type == Value && m_token.endsWith(QStringLiteral("\\"))) {
            m_token.chop(1);
            m_token.append(QLatin1Char(' '));
            report();
            mark();
            m_cls.resetToValue();
        }
        else {
            flush();
            mark();
            m_type = Syntax;
            m_cls.resetToNewLine();
        }
    }
private:
    TokenClass m_type;
    LineCounter m_counter;
    TokenClassifier m_cls;
    QString m_token;
    int m_tokenLine, m_tokenColumn;
};

Tokeniser::Tokeniser(const Tokeniser::LineEnding& lineEnding, QObject * parent) : QObject(parent), d_ptr(new TokeniserPrivate(lineEnding, this)) {}

Tokeniser::~Tokeniser()
{
    Q_D(Tokeniser);
    delete d;
}

void Tokeniser::receive(QChar c)
{
    Q_D(Tokeniser);
    d->push(c);
}

void Tokeniser::receivePair(QChar fst, QChar snd)
{
    Q_D(Tokeniser);
    d->pushPair(fst, snd);
}

void Tokeniser::end(void)
{
    Q_D(Tokeniser);
    d->finish();
}
