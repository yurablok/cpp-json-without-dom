// C++ JSON without DOM
//
// C++17(11*) callback-based DOM-less parsing and generating JSON messages.
//
// Author: Yurii Blok
// License: BSL-1.0
// https://github.com/yurablok/cpp-json-without-dom
// History:
// v0.8 2023-Jan-13     Optimized json_writer by 20%.
// v0.7 2022-Apr-21     json_reader now works with two types of handlers (object and array).
// v0.6 2022-Mar-24     Proper handling of `double` in `charconv` support in GCC.
// v0.5 2021-Dec-24     Autoconvert `nan` and `inf` to `null` in json_writer.
// v0.4 2021-Dec-15     Return void in handlers for json_reader.
// v0.3 2021-Nov-17     Single-line branches.
// v0.2 2021-Nov-15     C++11 support by using of third-party libs.
// v0.1 2021-Aug-23     First release.

#pragma once
#include <string>
#include <functional>
#include <type_traits>
#include <cmath>

#if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#   define CJWD_CPP_LIB_CHARCONV
#   define CJWD_CPP_LIB_CHARCONV_FLOAT
#   define CJWD_CPP_LIB_VARIANT
#elif __cplusplus >= 201703L
#   if defined(__GNUG__)
#       if __GNUC__ >= 8 && __GNUC_MINOR__ >= 1
#           define CJWD_CPP_LIB_CHARCONV
#           if defined(__cpp_lib_to_chars) || defined(_GLIBCXX_HAVE_USELOCALE)
#               define CJWD_CPP_LIB_CHARCONV_FLOAT
#           endif
#       endif
#       define CJWD_CPP_LIB_VARIANT
#   else
#       define CJWD_CPP_LIB_CHARCONV
#       define CJWD_CPP_LIB_CHARCONV_FLOAT
#       define CJWD_CPP_LIB_VARIANT
#   endif
#endif

#if defined(CJWD_CPP_LIB_CHARCONV)
#   include <string_view>
#   include <charconv>
#else
#   include "string_view.hpp" // https://github.com/martinmoene/string-view-lite
namespace std {
    using string_view = nonstd::string_view;
}
#endif

#if defined(CJWD_CPP_LIB_VARIANT)
#   include <variant>
#else
#   include "variant.hpp" // https://github.com/mpark/variant
namespace std {
    template <typename... args_t>
    using variant = mpark::variant<args_t...>;
    
    template <std::size_t I, typename... args_t>
    inline mpark::variant_alternative_t<I, variant<args_t...>>& get(variant<args_t...>& v) {
        return mpark::get<I>(v);
    }
    template <std::size_t I, typename... args_t>
    inline mpark::variant_alternative_t<I, variant<args_t...>>&& get(variant<args_t...>&& v) {
        return mpark::get<I>(std::move(v));
    }
    template <std::size_t I, typename... args_t>
    inline const mpark::variant_alternative_t<I, variant<args_t...>>& get(const variant<args_t...>& v) {
        return mpark::get<I>(v);
    }
    template <std::size_t I, typename... args_t>
    inline const mpark::variant_alternative_t<I, variant<args_t...>>&& get(const variant<args_t...>&& v) {
        return mpark::get<I>(std::move(v));
    }
    template <typename T, typename... args_t>
    inline T& get(variant<args_t...>& v) {
        return mpark::get<T>(v);
    }
    template <typename T, typename... args_t>
    inline T&& get(variant<args_t...>&& v) {
        return mpark::get<T>(std::move(v));
    }
    template <typename T, typename... args_t>
    inline const T& get(const variant<args_t...>& v) {
        return mpark::get<T>(v);
    }
    template <typename T, typename... args_t>
    inline const T&& get(const variant<args_t...>&& v) {
        return mpark::get<T>(std::move(v));
    }
}
#endif


struct json_reader {
    const char* begin = nullptr;
    const char* end = nullptr; // begin + size
    const char* error = nullptr; // check after parse
    uint8_t rootType = 0; // 1 - object, 2 - array

    void operator=(const std::string_view json) {
        begin = json.data();
        end = json.data() + json.size();
        error = nullptr;
        rootType = 0;
        while (begin < end) {
            switch (*begin++) {
            case '{':
                rootType = 1;
                return;
            case '[':
                rootType = 2;
                return;
            default:
                break;
            }
        }
        error = begin;
    }

    struct object_t {};
    struct array_t {};
    struct null_t {};
    using key_t = std::string_view;
    enum idx : uint8_t {
        number_idx, // use strings for big integers
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

    bool is_object() const {
        return rootType == 1;
    }
    bool is_array() const {
        return rootType == 2;
    }

    void parse(std::function<void(key_t key, const value_t& value)> handler) {
        if (error != nullptr) {
            return;
        }
        enum class steps : uint8_t {
            next,
            key,
            colon,
            value,
            number,
            string,
            comment,
        } step = steps::next;

        const char* beginStr = nullptr;
        bool isPrevEscape = false;
        bool isStringWithEscape = false;
        key_t key;
        std::string keyStr;
        value_t value;
        std::string valueStr;
        for (; begin < end; ++begin) {
            switch (step) {
            case steps::next:
                switch (*begin) {
                case '}':
                    return;
                case ',':
                    break;
                case '"':
                    step = steps::key;
                    beginStr = begin + 1;
                    break;
                case '/':
                    step = steps::comment;
                    beginStr = begin;
                    break;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    error = begin;
                    return;
                }
                break;
            case steps::key:
                if (isPrevEscape) {
                    isPrevEscape = false;
                    if (handler) {
                        keyStr.push_back(*begin);
                    }
                    break;
                }
                switch (*begin) {
                case '"':
                    step = steps::colon;
                    //if (!handler) {
                    //    break;
                    //}
                    if (isStringWithEscape) {
                        key = keyStr;
                    }
                    else {
                        key = std::string_view(beginStr, begin - beginStr);
                    }
                    break;
                case '\\':
                    isPrevEscape = true;
                    if (!!handler & !isStringWithEscape) {
                        isStringWithEscape = true;
                        if (beginStr != begin) {
                            keyStr = std::string(beginStr, begin - beginStr);
                        }
                    }
                    break;
                default:
                    if (!!handler & isStringWithEscape) {
                        keyStr.push_back(*begin);
                    }
                    break;
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
                case '{': {
                    value.emplace<object_idx>();
                    auto beginBefore = ++begin;
                    if (handler) {
                        handler(key, value);
                    }
                    if (begin == beginBefore) {
                        parse(std::function<void(key_t, const value_t&)>());
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    break;
                }
                case '[': {
                    value.emplace<array_idx>();
                    auto beginBefore = ++begin;
                    if (handler) {
                        handler(key, value);
                    }
                    if (begin == beginBefore) {
                        parse(std::function<void(uint32_t, const value_t&)>());
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    break;
                }
                case '"':
                    step = steps::string;
                    beginStr = begin + 1;
                    isStringWithEscape = false;
                    break;
                case '-': case '+':
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
                            if (handler) {
                                value.emplace<null_idx>();
                                handler(key, value);
                            }
                            step = steps::next;
                            break;
                        }
                        else if (std::string_view(begin, 4) == "true") {
                            begin += 4 - 1;
                            if (handler) {
                                value.emplace<boolean_idx>(true);
                                handler(key, value);
                            }
                            step = steps::next;
                            break;
                        }
                        else if (std::string_view(begin, 5) == "false") {
                            begin += 5 - 1;
                            if (handler) {
                                value.emplace<boolean_idx>(false);
                                handler(key, value);
                            }
                            step = steps::next;
                            break;
                        }
                    }
                    error = begin;
                    return;
                }
                break;
            case steps::number:
                switch (*begin) {
                case '-': case '+':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case '.': case 'e': case 'E':
                    break;
                default:
                    double v = 0.0;
#                 if defined(CJWD_CPP_LIB_CHARCONV_FLOAT)
                    auto [ptr, ec] = std::from_chars(beginStr, begin, v);
                    if (ptr != begin || ec != std::errc()) {
                        error = ptr;
                        return;
                    }
#                 else
                    char format[8];
                    std::snprintf(format, sizeof(format), "%%%ulf",
                        static_cast<uint32_t>(begin - beginStr));
                    if (std::sscanf(beginStr, format, &v) != 1) {
                        error = beginStr;
                        return;
                    }
#                 endif
                    --begin;
                    if (handler) {
                        value.emplace<number_idx>(v);
                        handler(key, value);
                    }
                    step = steps::next;
                    break;
                }
                break;
            case steps::string:
                if (isPrevEscape) {
                    isPrevEscape = false;
                    if (handler) {
                        valueStr.push_back(*begin);
                    }
                    break;
                }
                switch (*begin) {
                case '"':
                    if (handler) {
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
                    step = steps::next;
                    break;
                case '\\':
                    isPrevEscape = true;
                    if (!!handler & !isStringWithEscape) {
                        isStringWithEscape = true;
                        if (beginStr != begin) {
                            valueStr = std::string(beginStr, begin - beginStr);
                        }
                    }
                    break;
                default:
                    if (!!handler & isStringWithEscape) {
                        valueStr.push_back(*begin);
                    }
                    break;
                }
                break;
            case steps::comment:
                switch (*begin) {
                case '\r': case '\n':
                    step = steps::next;
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

    void parse(std::function<void(uint32_t index, const value_t& value)> handler) {
        if (error != nullptr) {
            return;
        }
        enum class steps : uint8_t {
            next,
            number,
            string,
            comment,
        } step = steps::next;

        const char* beginStr = nullptr;
        bool isPrevEscape = false;
        bool isStringWithEscape = false;
        uint32_t index = 0;
        value_t value;
        std::string valueStr;
        for (; begin < end; ++begin) {
            switch (step) {
            case steps::next:
                switch (*begin) {
                case ']':
                    return;
                case ',':
                    break;
                case '{': {
                    value.emplace<object_idx>();
                    auto beginBefore = ++begin;
                    if (handler) {
                        handler(index, value);
                    }
                    if (begin == beginBefore) {
                        parse(std::function<void(key_t, const value_t&)>());
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    ++index;
                    break;
                }
                case '[': {
                    value.emplace<array_idx>();
                    auto beginBefore = ++begin;
                    if (handler) {
                        handler(index, value);
                    }
                    if (begin == beginBefore) {
                        parse(std::function<void(uint32_t, const value_t&)>());
                    }
                    if (error != nullptr) {
                        return;
                    }
                    step = steps::next;
                    ++index;
                    break;
                }
                case '-': case '+':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    step = steps::number;
                    beginStr = begin;
                    break;
                case '"':
                    step = steps::string;
                    beginStr = begin + 1;
                    isStringWithEscape = false;
                    break;
                case '/':
                    step = steps::comment;
                    beginStr = begin;
                    break;
                case ' ': case '\t': case '\r': case '\n':
                    break;
                default:
                    if (end - begin >= 5) {
                        if (std::string_view(begin, 4) == "null") {
                            begin += 4 - 1;
                            if (handler) {
                                value.emplace<null_idx>();
                                handler(index, value);
                            }
                            step = steps::next;
                            ++index;
                            break;
                        }
                        else if (std::string_view(begin, 4) == "true") {
                            begin += 4 - 1;
                            if (handler) {
                                value.emplace<boolean_idx>(true);
                                handler(index, value);
                            }
                            step = steps::next;
                            ++index;
                            break;
                        }
                        else if (std::string_view(begin, 5) == "false") {
                            begin += 5 - 1;
                            if (handler) {
                                value.emplace<boolean_idx>(false);
                                handler(index, value);
                            }
                            step = steps::next;
                            ++index;
                            break;
                        }
                    }
                    error = begin;
                    return;
                }
                break;
            case steps::number:
                switch (*begin) {
                case '-': case '+':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case '.': case 'e': case 'E':
                    break;
                default:
                    double v = 0.0;
#                 if defined(CJWD_CPP_LIB_CHARCONV_FLOAT)
                    auto [ptr, ec] = std::from_chars(beginStr, begin, v);
                    if (ptr != begin || ec != std::errc()) {
                        error = ptr;
                        return;
                    }
#                 else
                    char format[8];
                    std::snprintf(format, sizeof(format), "%%%ulf",
                        static_cast<uint32_t>(begin - beginStr));
                    if (std::sscanf(beginStr, format, &v) != 1) {
                        error = beginStr;
                        return;
                    }
#                 endif
                    --begin;
                    if (handler) {
                        value.emplace<number_idx>(v);
                        handler(index, value);
                    }
                    step = steps::next;
                    ++index;
                    break;
                }
                break;
            case steps::string:
                if (isPrevEscape) {
                    isPrevEscape = false;
                    if (handler) {
                        valueStr.push_back(*begin);
                    }
                    break;
                }
                switch (*begin) {
                case '"':
                    if (handler) {
                        if (isStringWithEscape) {
                            value.emplace<string_idx>(valueStr);
                        }
                        else {
                            value.emplace<string_idx>(std::string_view(beginStr, begin - beginStr));
                        }
                        handler(index, value);
                        valueStr.clear();
                    }
                    step = steps::next;
                    ++index;
                    break;
                case '\\':
                    isPrevEscape = true;
                    if (!!handler & !isStringWithEscape) {
                        isStringWithEscape = true;
                        if (beginStr != begin) {
                            valueStr = std::string(beginStr, begin - beginStr);
                        }
                    }
                    break;
                default:
                    if (!!handler & isStringWithEscape) {
                        valueStr.push_back(*begin);
                    }
                    break;
                }
                break;
            case steps::comment:
                switch (*begin) {
                case '\r': case '\n':
                    step = steps::next;
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
    size_t lastComma = 0;
    uint8_t level = 0;
    bool singleLine = false;
    bool isPrevKey = false;
    void tab(const bool removeComma) {
        if (isPrevKey) {
            isPrevKey = false;
            return;
        }
        if (removeComma & lastComma > 0) {
            buffer[lastComma] = ' ';
        }
        lastComma = 0;
        if (singleLine) {
            buffer.push_back(' ');
            return;
        }
        buffer.push_back('\n');
        buffer.resize(buffer.size() + static_cast<size_t>(level) * tabSize, ' ');
    }
    inline void append(const std::string_view str) {
        for (const char c : str) {
            buffer.push_back(c);
        }
    }
    void string(const std::string_view str) {
        for (const char c : str) {
            switch (c - 8) {
            //case '/':
            case '\b' - 8: // 08
                append("\\b");
                break;
            case '\t' - 8: // 09
                append("\\t");
                break;
            case '\n' - 8: // 10
                append("\\n");
                break;
            case 11 - 8: //   11
                append("\\?");
                break;
            case '\f' - 8: // 12
                append("\\f");
                break;
            case '\r' - 8: // 13
                append("\\r");
                break;
            case '"' - 8: //  34
                append("\\\"");
                break;
            case '\\' - 8: // 92
                append("\\\\");
                break;
            default:
                buffer.push_back(c);
                break;
            }
        }
    }
public:
    enum class flags : uint8_t {
        none        = 0x00,
        single_line = 0x01,
    };
    uint8_t tabSize = 2;
    std::string buffer;

    struct object_t;
    struct array_t;
    struct value_t {
        object_t object(std::function<void(object_t json)> handler,
                const flags flags_ = flags::none) {
            writer->tab(false);
            writer->buffer.push_back('{');
            
            if (handler) {
                ++writer->level;
                if (flags_ == flags::single_line & !writer->singleLine) {
                    writer->singleLine = true;
                    handler({ writer });
                    --writer->level;
                    writer->tab(true);
                    writer->buffer.pop_back();
                    writer->singleLine = false;
                }
                else {
                    handler({ writer });
                    --writer->level;
                    writer->tab(true);
                }
            }
            writer->append("},");
            writer->lastComma = writer->buffer.size() - 1;
            return { writer };
        }
        object_t array(std::function<void(array_t json)> handler,
                const flags flags_ = flags::none) {
            writer->tab(false);
            writer->buffer.push_back('[');

            if (handler) {
                ++writer->level;
                if (flags_ == flags::single_line & !writer->singleLine) {
                    writer->singleLine = true;
                    handler({ writer });
                    --writer->level;
                    writer->tab(true);
                    writer->buffer.pop_back();
                    writer->singleLine = false;
                }
                else {
                    handler({ writer });
                    --writer->level;
                    writer->tab(true);
                }
            }
            writer->append("],");
            writer->lastComma = writer->buffer.size() - 1;
            return { writer };
        }
        template <typename bool_t,
            typename = typename std::enable_if<std::is_same<bool_t, bool>::value>::type>
        object_t value(const bool_t boolean) {
            writer->tab(false);
            if (boolean) {
                writer->append("true,");
            }
            else {
                writer->append("false,");
            }
            writer->lastComma = writer->buffer.size() - 1;
            return { writer };
        }
        object_t value(std::nullptr_t = nullptr) {
            writer->tab(false);
            writer->append("null,");
            writer->lastComma = writer->buffer.size() - 1;
            return { writer };
        }
        object_t value(const std::string_view string) {
            writer->tab(false);
            writer->buffer.push_back('"');
            writer->string(string);
            writer->append("\",");
            writer->lastComma = writer->buffer.size() - 1;
            return { writer };
        }
#     ifdef __cpp_lib_char8_t
        object_t value(const std::u8string_view string) {
            return value(std::string_view(reinterpret_cast<const char*>(string.data()), string.size()));
        }
#     endif // __cpp_lib_char8_t
        object_t value(const double number) {
            if (std::isnan(number) | std::isinf(number)) {
                return value(nullptr);
            }
            writer->tab(false);
            const auto size = writer->buffer.size();
            writer->buffer.resize(size + 32);
#         if defined(CJWD_CPP_LIB_CHARCONV_FLOAT)
            const auto [ptr, ec] = std::to_chars(
                writer->buffer.data() + size,
                writer->buffer.data() + size + 32,
                number
            );
            if (ec == std::errc()) {
                writer->buffer.resize(ptr - writer->buffer.data());
            }
#         else
            const int32_t length = std::trunc(number) == number
                ? std::snprintf(const_cast<char*>(
                    writer->buffer.data() + size), 32, "%.0f", number)
                : std::snprintf(const_cast<char*>(
                    writer->buffer.data() + size), 32, "%.8e", number);
            if (length > 0) {
                writer->buffer.resize(size + length);
            }
#         endif
            else {
                writer->isPrevKey = true; // skip tab()
                value(nullptr);
            }
            writer->lastComma = writer->buffer.size();
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
            writer->append("\": ");
            writer->isPrevKey = true;
            return { writer };
        }
        object_t comment(const std::string_view line) {
            if (writer->singleLine) {
                return { writer };
            }
            writer->tab(false);
            writer->append("//");
            writer->string(line);
            return { writer };
        }

        object_t(json_writer* writer_) : writer(writer_) {}
    private:
        json_writer* writer = nullptr;
    };
    struct array_t {
        array_t& object(std::function<void(object_t json)> handler,
                const flags flags_ = flags::none) {
            writer->tab(false);
            writer->buffer.push_back('{');

            if (handler) {
                ++writer->level;
                if (flags_ == flags::single_line & !writer->singleLine) {
                    writer->singleLine = true;
                    handler({ writer });
                    --writer->level;
                    writer->tab(true);
                    writer->buffer.pop_back();
                    writer->singleLine = false;
                }
                else {
                    handler({ writer });
                    --writer->level;
                    writer->tab(true);
                }
            }
            writer->append("},");
            writer->lastComma = writer->buffer.size() - 1;
            return *this;
        }
        array_t& array(std::function<void(array_t json)> handler,
                const flags flags_ = flags::none) {
            writer->tab(false);
            writer->buffer.push_back('[');

            if (handler) {
                ++writer->level;
                if (flags_ == flags::single_line & !writer->singleLine) {
                    writer->singleLine = true;
                    handler(*this);
                    --writer->level;
                    writer->tab(true);
                    writer->buffer.pop_back();
                    writer->singleLine = false;
                }
                else {
                    handler(*this);
                    --writer->level;
                    writer->tab(true);
                }
            }
            writer->append("],");
            writer->lastComma = writer->buffer.size() - 1;
            return *this;
        }
        template <typename bool_t,
            typename = typename std::enable_if<std::is_same<bool_t, bool>::value>::type>
        array_t& value(const bool_t boolean) {
            writer->tab(false);
            if (boolean) {
                writer->append("true,");
            }
            else {
                writer->append("false,");
            }
            writer->lastComma = writer->buffer.size() - 1;
            return *this;
        }
        array_t& value(std::nullptr_t = nullptr) {
            writer->tab(false);
            writer->append("null,");
            writer->lastComma = writer->buffer.size() - 1;
            return *this;
        }
        array_t& value(const std::string_view string) {
            writer->tab(false);
            writer->buffer.push_back('"');
            writer->string(string);
            writer->append("\",");
            writer->lastComma = writer->buffer.size() - 1;
            return *this;
        }
#     ifdef __cpp_lib_char8_t
        array_t& value(const std::u8string_view string) {
            return value(std::string_view(reinterpret_cast<const char*>(string.data()), string.size()));
        }
#     endif // __cpp_lib_char8_t
        array_t& value(const double number) {
            if (std::isnan(number) | std::isinf(number)) {
                return value(nullptr);
            }
            writer->tab(false);
            const auto size = writer->buffer.size();
            writer->buffer.resize(size + 32);
#         if defined(CJWD_CPP_LIB_CHARCONV_FLOAT)
            const auto [ptr, ec] = std::to_chars(
                writer->buffer.data() + size,
                writer->buffer.data() + size + 32,
                number
            );
            if (ec == std::errc()) {
                writer->buffer.resize(ptr - writer->buffer.data());
            }
#         else
            const int32_t length = std::trunc(number) == number
                ? std::snprintf(const_cast<char*>(
                    writer->buffer.data() + size), 32, "%.0f", number)
                : std::snprintf(const_cast<char*>(
                    writer->buffer.data() + size), 32, "%.8e", number);
            if (length > 0) {
                writer->buffer.resize(size + length);
            }
#         endif
            else {
                writer->isPrevKey = true; // skip tab()
                value(nullptr);
            }
            writer->lastComma = writer->buffer.size();
            writer->buffer.push_back(',');
            return *this;
        }
        array_t& comment(const std::string_view line) {
            if (writer->singleLine) {
                return *this;
            }
            writer->tab(false);
            writer->append("//");
            writer->string(line);
            return *this;
        }

        array_t(json_writer* writer_) : writer(writer_) {}
    private:
        json_writer* writer = nullptr;
    };

    object_t object(std::function<void(object_t json)> handler,
            const flags flags_ = flags::none) {
        buffer.clear();
        buffer.push_back('{');

        if (flags_ == flags::single_line) {
            singleLine = true;
        }
        else {
            singleLine = false;
        }
        if (handler) {
            ++level;
            handler({ this });
            --level;
        }
        tab(true);
        buffer.push_back('}');
        return { this };
    }
    array_t array(std::function<void(array_t json)> handler,
            const flags flags_ = flags::none) {
        buffer.clear();
        buffer.push_back('[');

        if (flags_ == flags::single_line) {
            singleLine = true;
        }
        else {
            singleLine = false;
        }
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
