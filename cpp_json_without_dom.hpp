// C++ JSON without DOM
//
// C++17 callback-based DOM-less parsing and generating JSON messages.
//
// Author: Yurii Blok
// License: BSL-1.0
// https://github.com/yurablok/cpp-json-without-dom
// History:
// v0.1 2021-Aug-23     First release.

#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>
#include <functional>
#include <optional>
#include <charconv>

struct json_reader {
    const char* begin = nullptr;
    const char* end = nullptr; // begin + size
    const char* error = nullptr; // check after parse
    void operator=(const std::string_view json) {
        begin = json.data();
        end = json.data() + json.size();
        error = nullptr;
    }

    struct object_t {};
    struct array_t {};
    struct null_t {};
    using key_t = std::string_view;
    enum idx : uint8_t {
        number_idx, // use string for big integer
        string_idx,
        boolean_idx,
        array_idx,
        object_idx,
        null_idx,
    };
    struct value_t : public std::variant<
            double, std::string_view, bool, array_t, object_t, null_t> {
        bool is_number() const {
            return index() == number_idx;
        }
        double as_number() const {
            return std::get<number_idx>(*this);
        }
        bool is_string() const {
            return index() == string_idx;
        }
        const std::string_view as_string() const {
            return std::get<string_idx>(*this);
        }
        bool is_boolean() const {
            return index() == boolean_idx;
        }
        bool as_boolean() const {
            return std::get<boolean_idx>(*this);
        }
        bool is_array() const {
            return index() == array_idx;
        }
        bool is_object() const {
            return index() == object_idx;
        }
        bool is_null() const {
            return index() == null_idx;
        }
    };

    // handler -> true  if processed
    // handler -> false if skipped
    void parse(std::function<bool(key_t key, const value_t& value)> handler) {
        if (error != nullptr) {
            return;
        }
        enum class steps : uint8_t {
            undefined,
            key,
            colon,
            value,
            number,
            string,
            next,
            comment,
        } step = steps::undefined;

        const char* beginStr = nullptr;
        bool isObject = false;
        bool isKey = false;
        bool isPrevEscape = false;
        bool isStringWithEscape = false;
        key_t key;
        std::string keyStr;
        value_t value;
        std::string valueStr;
        for (; begin < end; ++begin) {
            switch (step) {
            case steps::undefined:
                switch (*begin) {
                case '{':
                    step = steps::key;
                    isObject = true;
                    break;
                case '[':
                    step = steps::value;
                    isObject = false;
                    break;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    error = begin;
                    return;
                }
                break;
            case steps::key:
                switch (*begin) {
                case '/':
                    step = steps::comment;
                    beginStr = begin;
                    break;
                case '{':
                    value.emplace<object_idx>();
                    if (!handler || !handler(key, value)) {
                        parse(nullptr);
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    break;
                case '}':
                    return;
                case '"':
                    step = steps::string;
                    beginStr = begin + 1;
                    isKey = true;
                    isStringWithEscape = false;
                    break;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    error = begin;
                    return;
                }
                break;
            case steps::colon:
                switch (*begin) {
                case ':':
                    step = steps::value;
                    break;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    error = begin;
                    return;
                }
                break;
            case steps::value:
                switch (*begin) {
                case '/':
                    if (isObject) {
                        error = begin;
                        return;
                    }
                    step = steps::comment;
                    beginStr = begin;
                    break;
                case '{':
                    value.emplace<object_idx>();
                    if (!handler || !handler(key, value)) {
                        parse(nullptr);
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    break;
                case '}':
                    return;
                case '[':
                    value.emplace<array_idx>();
                    if (!handler || !handler(key, value)) {
                        parse(nullptr);
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    break;
                case ']':
                    return;
                case '"':
                    step = steps::string;
                    beginStr = begin + 1;
                    isStringWithEscape = false;
                    break;
                case '-': // case '+':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    step = steps::number;
                    beginStr = begin;
                    break;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    if (end - begin >= 5) {
                        if (std::string_view(begin, 4) == "null") {
                            begin += 4 - 1;
                            step = steps::next;
                            if (!handler) {
                                continue;
                            }
                            value.emplace<null_idx>();
                            handler(key, value);
                            break;
                        }
                        else if (std::string_view(begin, 4) == "true") {
                            begin += 4 - 1;
                            step = steps::next;
                            if (!handler) {
                                continue;
                            }
                            value.emplace<boolean_idx>(true);
                            handler(key, value);
                            break;
                        }
                        else if (std::string_view(begin, 5) == "false") {
                            begin += 5 - 1;
                            step = steps::next;
                            if (!handler) {
                                continue;
                            }
                            value.emplace<boolean_idx>(false);
                            handler(key, value);
                            break;
                        }
                    }
                    error = begin;
                    return;
                }
                break;
            case steps::number:
                switch (*begin) {
                case '-': // case '+':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case '.': case 'e': case 'E':
                    break;
                default:
                    double v = 0.0;
                    auto [ptr, ec] = std::from_chars(beginStr, begin, v);
                    if (ptr != begin || ec != std::errc()) {
                        error = ptr;
                        return;
                    }
                    step = steps::next;
                    --begin;
                    if (!handler) {
                        continue;
                    }
                    value.emplace<number_idx>(v);
                    handler(key, value);
                    break;
                }
                break;
            case steps::string:
                if (isPrevEscape) {
                    isPrevEscape = false;
                    if (!handler) {
                        break;
                    }
                    if (isKey) {
                        keyStr.push_back(*begin);
                    }
                    else {
                        valueStr.push_back(*begin);
                    }
                    break;
                }
                switch (*begin) {
                case '"': {
                    const bool isKeyCopy = isKey;
                    if (isKey) {
                        step = steps::colon;
                        isKey = false;
                    }
                    else {
                        step = steps::next;
                    }
                    if (!handler) {
                        break;
                    }
                    if (isKeyCopy) {
                        if (isStringWithEscape) {
                            key = keyStr;
                        }
                        else {
                            key = std::string_view(beginStr, begin - beginStr);
                        }
                    }
                    else {
                        if (isStringWithEscape) {
                            value.emplace<string_idx>(valueStr);
                        }
                        else {
                            value.emplace<string_idx>(std::string_view(beginStr, begin - beginStr));
                        }
                        handler(key, value);
                        keyStr.clear();
                        valueStr.clear();
                    }
                    break;
                }
                case '\\':
                    isPrevEscape = true;
                    if (!handler) {
                        break;
                    }
                    if (!isStringWithEscape) {
                        isStringWithEscape = true;
                        if (beginStr != begin) {
                            if (isKey) {
                                keyStr = std::string(beginStr, begin - beginStr);
                            }
                            else {
                                valueStr = std::string(beginStr, begin - beginStr);
                            }
                        }
                    }
                    break;
                default:
                    if (!handler) {
                        break;
                    }
                    if (isStringWithEscape) {
                        if (isKey) {
                            keyStr.push_back(*begin);
                        }
                        else {
                            valueStr.push_back(*begin);
                        }
                    }
                    break;
                }
                break;
            case steps::next:
                switch (*begin) {
                case ',':
                    step = isObject ? steps::key : steps::value;
                    break;
                case '}':
                    if (!isObject) {
                        error = begin;
                        return;
                    }
                    return;
                case ']':
                    if (isObject) {
                        error = begin;
                        return;
                    }
                    return;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    error = begin;
                    return;
                }
                break;
            case steps::comment:
                switch (*begin) {
                case '\r': case '\n':
                    step = isObject ? steps::key : steps::value;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
        if (step == steps::string) {
            error = begin;
        }
    }
};

struct json_writer {
private:
    uint32_t lastComma = 0;
    uint8_t level = 0;
    bool isPrevKey = false;
    void tab(const bool removeComma) {
        if (isPrevKey) {
            isPrevKey = false;
            return;
        }
        if (removeComma && lastComma) {
            buffer[lastComma] = ' ';
            lastComma = 0;
        }
        buffer.push_back('\n');
        for (uint8_t i = 0; i < level * tabSize; ++i) {
            buffer.push_back(' ');
        }
    }
    void string(const std::string_view str) {
        for (const auto c : str) {
            switch (c) {
            case '"':
                buffer.push_back('\\');
                buffer.push_back('"');
                break;
            case '\\':
                buffer.push_back('\\');
                buffer.push_back('\\');
                break;
            //case '/':
            case '\t':
                buffer.push_back('\\');
                buffer.push_back('t');
                break;
            case '\n':
                buffer.push_back('\\');
                buffer.push_back('n');
                break;
            case '\r':
                buffer.push_back('\\');
                buffer.push_back('r');
                break;
            case '\f':
                buffer.push_back('\\');
                buffer.push_back('f');
                break;
            case '\b':
                buffer.push_back('\\');
                buffer.push_back('b');
                break;
            default:
                buffer.push_back(c);
                break;
            }
        }
    }
public:
    uint8_t tabSize = 2;
    std::string buffer;

    struct object_t;
    struct array_t;
    struct value_t {
        object_t object(std::function<void(object_t json)> handler) {
            writer->tab(false);
            writer->buffer.push_back('{');
            
            if (handler) {
                ++writer->level;
                handler({ writer });
                --writer->level;
            }
            writer->tab(true);
            writer->buffer.push_back('}');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return { writer };
        }
        object_t array(std::function<void(array_t json)> handler) {
            writer->tab(false);
            writer->buffer.push_back('[');

            if (handler) {
                ++writer->level;
                handler({ writer });
                --writer->level;
            }
            writer->tab(true);
            writer->buffer.push_back(']');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return { writer };
        }
        template <typename bool_t,
            std::enable_if_t<std::is_same_v<bool_t, bool>, int> = 0>
        object_t value(const bool_t boolean) {
            writer->tab(false);
            if (boolean) {
                writer->buffer.push_back('t');
                writer->buffer.push_back('r');
                writer->buffer.push_back('u');
                writer->buffer.push_back('e');
            }
            else {
                writer->buffer.push_back('f');
                writer->buffer.push_back('a');
                writer->buffer.push_back('l');
                writer->buffer.push_back('s');
                writer->buffer.push_back('e');
            }
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return { writer };
        }
        object_t value(std::nullptr_t = nullptr) {
            writer->tab(false);
            writer->buffer.push_back('n');
            writer->buffer.push_back('u');
            writer->buffer.push_back('l');
            writer->buffer.push_back('l');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return { writer };
        }
        object_t value(const std::string_view string) {
            writer->tab(false);
            writer->buffer.push_back('"');
            writer->string(string);
            writer->buffer.push_back('"');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return { writer };
        }
        object_t value(const double number) {
            writer->tab(false);
            const auto size = writer->buffer.size();
            writer->buffer.resize(size + 32);
            const auto [ptr, ec] = std::to_chars(
                writer->buffer.data() + size,
                writer->buffer.data() + size + 32,
                number
            );
            if (ec == std::errc()) {
                writer->buffer.resize(ptr - writer->buffer.data());
            }
            else {
                writer->isPrevKey = true; // skip tab()
                value(nullptr);
            }
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return { writer };
        }

        value_t(json_writer* writer_) : writer(writer_) {}
    private:
        json_writer* writer = nullptr;
    };
    struct object_t {
        value_t key(const std::string_view string) {
            writer->tab(false);
            writer->buffer.push_back('"');
            writer->string(string);
            writer->buffer.push_back('"');
            writer->buffer.push_back(':');
            writer->buffer.push_back(' ');
            writer->isPrevKey = true;
            return { writer };
        }
        object_t comment(const std::string_view line) {
            writer->tab(false);
            writer->buffer.push_back('/');
            writer->buffer.push_back('/');
            writer->string(line);
            return { writer };
        }

        object_t(json_writer* writer_) : writer(writer_) {}
    private:
        json_writer* writer = nullptr;
    };
    struct array_t {
        array_t& object(std::function<void(object_t json)> handler) {
            writer->tab(false);
            writer->buffer.push_back('{');

            if (handler) {
                ++writer->level;
                handler({ writer });
                --writer->level;
            }
            writer->tab(true);
            writer->buffer.push_back('}');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return *this;
        }
        array_t& array(std::function<void(array_t json)> handler) {
            writer->tab(false);
            writer->buffer.push_back('[');

            if (handler) {
                ++writer->level;
                handler(*this);
                --writer->level;
            }
            writer->tab(true);
            writer->buffer.push_back(']');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return *this;
        }
        template <typename bool_t,
            std::enable_if_t<std::is_same_v<bool_t, bool>, int> = 0>
        array_t& value(const bool_t boolean) {
            writer->tab(false);
            if (boolean) {
                writer->buffer.push_back('t');
                writer->buffer.push_back('r');
                writer->buffer.push_back('u');
                writer->buffer.push_back('e');
            }
            else {
                writer->buffer.push_back('f');
                writer->buffer.push_back('a');
                writer->buffer.push_back('l');
                writer->buffer.push_back('s');
                writer->buffer.push_back('e');
            }
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return *this;
        }
        array_t& value(std::nullptr_t = nullptr) {
            writer->tab(false);
            writer->buffer.push_back('n');
            writer->buffer.push_back('u');
            writer->buffer.push_back('l');
            writer->buffer.push_back('l');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return *this;
        }
        array_t& value(const std::string_view string) {
            writer->tab(false);
            writer->buffer.push_back('"');
            writer->string(string);
            writer->buffer.push_back('"');
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return *this;
        }
        array_t& value(const double number) {
            writer->tab(false);
            const auto size = writer->buffer.size();
            writer->buffer.resize(size + 32);
            const auto [ptr, ec] = std::to_chars(
                writer->buffer.data() + size,
                writer->buffer.data() + size + 32,
                number
            );
            if (ec == std::errc()) {
                writer->buffer.resize(ptr - writer->buffer.data());
            }
            else {
                writer->isPrevKey = true; // skip tab()
                value(nullptr);
            }
            writer->lastComma = static_cast<uint32_t>(writer->buffer.size());
            writer->buffer.push_back(',');
            return *this;
        }
        array_t& comment(const std::string_view line) {
            writer->tab(false);
            writer->buffer.push_back('/');
            writer->buffer.push_back('/');
            writer->string(line);
            return *this;
        }

        array_t(json_writer* writer_) : writer(writer_) {}
    private:
        json_writer* writer = nullptr;
    };

    object_t object(std::function<void(object_t json)> handler) {
        buffer.clear();
        buffer.push_back('{');

        if (handler) {
            ++level;
            handler({ this });
            --level;
        }
        tab(true);
        buffer.push_back('}');
        return { this };
    }
    array_t array(std::function<void(array_t json)> handler) {
        buffer.clear();
        buffer.push_back('[');

        if (handler) {
            ++level;
            handler({ this });
            --level;
        }
        tab(true);
        buffer.push_back(']');
        return { this };
    }
};
