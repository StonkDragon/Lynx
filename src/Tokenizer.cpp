#include <LynxConf.hpp>

bool isValidIdentifier(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static bool isnumber(char c) {
    return (c >= '0' && c <= '9');
}

#define skipWhitespace() while (i < data.size() && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')) { if (data[i] == '\n') {line++; column = 0;} else column++; i++;  }
#define next(_r) ({ i++; column++; if (i >= data.size()) { LYNX_ERR << "Unexpected end of file" << std::endl; return _r; } data[i]; })
#define maybeNext(_r) ({ i++; column++; i < data.size() ? data[i] : '\0'; })
#define prev(_r) ({ i--; if (i < 0) { LYNX_ERR << "Unexpected start of file" << std::endl; return _r; } data[i]; })
#define nextChar(_r) ({ i++; column++; skipWhitespace(); if (i >= data.size()) { LYNX_ERR << "Unexpected end of file" << std::endl; return _r; } data[i]; })
#define peek() ({ skipWhitespace(); data[i]; })

bool Token::operator==(const Token& other) const {
    return this->type == other.type && this->value == other.value;
}

bool Token::operator!=(const Token& other) const {
    return !operator==(other);
}

std::vector<Token> tokenize(std::string file, std::string& data, int& i) {
    std::vector<Token> tokens;
    int line = 1;
    int column = 0;
    char c = peek();
    while (c != '\0') {
        Token token;
        token.file = file;
        token.line = line;
        token.column = column;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            skipWhitespace();
            c = peek();
            continue;
        } else if (c == '"') {
            token.type = Token::String;
            token.value = "";
            c = next({});
            while (c != '"' && c != '\0') {
                if (c == '\\') {
                    c = next({});
                    switch (c) {
                        case 'n': token.value += '\n'; break;
                        case 'r': token.value += '\r'; break;
                        case 't': token.value += '\t'; break;
                        case '0': token.value += '\0'; break;
                        case '\\': token.value += '\\'; break;
                        case '"': token.value += '"'; break;
                        default: LYNX_ERR << "Invalid escape sequence: \\" << c << std::endl; return tokens;
                    }
                } else {
                    token.value += c;
                }
                c = next({});
            }
            if (c == '\0') {
                LYNX_ERR << "Unexpected end of file" << std::endl;
                return tokens;
            }
        } else if (isnumber(c) || c == '-' || c == '+') {
            token.type = Token::Number;
            token.value = "";
            while (isnumber(c) || c == '.' || c == '-' || c == '+') {
                token.value += c;
                c = next({});
            }
            prev({});
        } else if (c == '[') {
            token.type = Token::ListStart;
            token.value = "[";
        } else if (c == ']') {
            token.type = Token::ListEnd;
            token.value = "]";
        } else if (c == '{') {
            token.type = Token::CompoundStart;
            token.value = "{";
        } else if (c == '}') {
            token.type = Token::CompoundEnd;
            token.value = "}";
        } else if (c == '(') {
            token.type = Token::BlockStart;
            token.value = "(";
        } else if (c == ')') {
            token.type = Token::BlockEnd;
            token.value = ")";
        } else if (c == '.') {
            token.type = Token::Dot;
            token.value = ".";
        } else if (c == '=') {
            token.type = Token::Assign;
            token.value = "=";
        } else if (c == ':') {
            token.type = Token::Is;
            token.value = ":";
        } else if (isValidIdentifier(c)) {
            token.type = Token::Identifier;
            token.value = "";
            while (isValidIdentifier(c)) {
                token.value += c;
                c = next({});
            }
            prev({});
        } else {
            LYNX_ERR << "Invalid character: " << c << std::endl;
            return tokens;
        }
        tokens.push_back(token);
        c = maybeNext({});
    }
    return tokens;
}
