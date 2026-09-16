#pragma once

// Strict JSON validator.
//
// The save writer emits JSON by hand. It once produced a trailing comma after
// the equipment array, which no external tool would accept but which the
// project's own tolerant scanner happily read back. Validating the output here
// catches that whole class of bug without a JSON dependency.

#include <cctype>
#include <string>

namespace json_check {

class Validator {
public:
    explicit Validator(const std::string& text) : text_(text) {}

    // Returns true if the whole input is exactly one well-formed JSON value.
    bool validate(std::string& error) {
        skip_whitespace();
        if (!parse_value()) { error = error_; return false; }
        skip_whitespace();
        if (pos_ != text_.size()) {
            error = "trailing content at offset " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    bool bad(const std::string& what) {
        if (error_.empty()) {
            error_ = what + " at offset " + std::to_string(pos_);
        }
        return false;
    }

    void skip_whitespace() {
        while (pos_ < text_.size() &&
               (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                text_[pos_] == '\n' || text_[pos_] == '\r')) {
            pos_++;
        }
    }

    bool consume(char expected) {
        if (pos_ < text_.size() && text_[pos_] == expected) { pos_++; return true; }
        return false;
    }

    bool parse_value() {
        if (pos_ >= text_.size()) return bad("unexpected end of input");
        switch (text_[pos_]) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return parse_string();
            case 't': return parse_literal("true");
            case 'f': return parse_literal("false");
            case 'n': return parse_literal("null");
            default:  return parse_number();
        }
    }

    bool parse_literal(const char* literal) {
        size_t len = std::string(literal).size();
        if (text_.compare(pos_, len, literal) != 0) return bad("invalid literal");
        pos_ += len;
        return true;
    }

    bool parse_object() {
        if (!consume('{')) return bad("expected '{'");
        skip_whitespace();
        if (consume('}')) return true;

        while (true) {
            skip_whitespace();
            if (!parse_string()) return bad("expected object key");
            skip_whitespace();
            if (!consume(':')) return bad("expected ':' after key");
            skip_whitespace();
            if (!parse_value()) return false;
            skip_whitespace();
            if (consume(',')) continue;         // another member must follow
            if (consume('}')) return true;
            return bad("expected ',' or '}' in object");
        }
    }

    bool parse_array() {
        if (!consume('[')) return bad("expected '['");
        skip_whitespace();
        if (consume(']')) return true;

        while (true) {
            skip_whitespace();
            if (!parse_value()) return false;
            skip_whitespace();
            if (consume(',')) continue;         // another element must follow
            if (consume(']')) return true;
            return bad("expected ',' or ']' in array");
        }
    }

    bool parse_string() {
        if (!consume('"')) return bad("expected '\"'");
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= text_.size()) return bad("unterminated escape");
                char esc = text_[pos_++];
                if (std::string("\"\\/bfnrt").find(esc) != std::string::npos) continue;
                if (esc == 'u') {
                    for (int i = 0; i < 4; ++i) {
                        if (pos_ >= text_.size() ||
                            !std::isxdigit(static_cast<unsigned char>(text_[pos_]))) {
                            return bad("invalid \\u escape");
                        }
                        pos_++;
                    }
                    continue;
                }
                return bad("invalid escape");
            }
            if (static_cast<unsigned char>(c) < 0x20) return bad("raw control character in string");
        }
        return bad("unterminated string");
    }

    bool parse_number() {
        size_t start = pos_;
        consume('-');
        if (pos_ >= text_.size()) return bad("expected number");
        if (consume('0')) {
            // leading zeros are not allowed
        } else {
            if (!std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                return bad("expected number");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        if (consume('.')) {
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                return bad("expected digits after '.'");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            pos_++;
            if (!consume('+')) consume('-');
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                return bad("expected exponent digits");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        return pos_ > start;
    }

    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;
};

inline bool is_valid(const std::string& text, std::string& error) {
    Validator validator(text);
    return validator.validate(error);
}

}
