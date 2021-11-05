#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <array>
#include <vector>
#include "switch-str/switch_str.hpp"

#include "../cpp_json_without_dom.hpp"

#include "minijson_reader/minijson_reader.hpp"
#include "minijson_writer/minijson_writer.hpp"

#include "nlohmann_json/single_include/nlohmann/json.hpp"

#include "rapidjson/include/rapidjson/document.h"
#include "rapidjson/include/rapidjson/prettywriter.h"
#include "rapidjson/include/rapidjson/stringbuffer.h"

// For Visual Studio:
// Force UTF-8 (No BOM) https://marketplace.visualstudio.com/items?itemName=1ndrew100.forceUTF8NoBOM
// https://github.com/yurablok/vs-force-utf8
// https://docs.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=vs-2019
// Project Properties -> Configuration Properties
// -> C/C++, Command Line -> Additional Options: /utf-8
// -> Advanced -> Character Set: Use Unicode Character Set
//
// For GCC + qmake:
// CONFIG += utf8_source
// QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
//
// For MSVC + qmake:
// msvc {
//   QMAKE_CFLAGS += -Qoption,cpp,--unicode_source_kind,UTF-8
//   QMAKE_CXXFLAGS += -Qoption,cpp,--unicode_source_kind,UTF-8
// }
static_assert(static_cast<uint8_t>(u8"🌍"[0]) == 0xF0, "Wrong UTF-8 config");
static_assert(static_cast<uint8_t>(u8"🌍"[1]) == 0x9F, "Wrong UTF-8 config");
static_assert(static_cast<uint8_t>(u8"🌍"[2]) == 0x8C, "Wrong UTF-8 config");
static_assert(static_cast<uint8_t>(u8"🌍"[3]) == 0x8D, "Wrong UTF-8 config");

void test() {
    json_writer writer;
    json_reader reader;
    {
        // {}
        writer.object(nullptr);
        reader = writer.buffer;
        reader.parse([](json_reader::key_t key, const json_reader::value_t& value) {
            assert(false);
            return true;
        });
        assert(reader.error == nullptr);
    }
    {
        // []
        writer.array(nullptr);
        reader = writer.buffer;
        reader.parse([](json_reader::key_t key, const json_reader::value_t& value) {
            assert(false);
            return true;
        });
        assert(reader.error == nullptr);
    }
    {
        // {
        //   "first": "second",
        //   "es\"ca\"pe": "es\"ca\"pe",
        //   "unicode": "🌍",
        //   "boolean": true,
        //   // comment first \n line
        //   // comment second line
        //   "special": null,
        //   "number": -123.456e-7
        // }
        writer.object([](json_writer::object_t json) {
            json
            .key("first").value("second")
            .key(R"(es"ca"pe)").value(R"(es"ca"pe)")
            .key("unicode").value(u8"🌍")
            .key("boolean").value(true)
            .comment(" comment first \n line")
            .comment(" comment second line")
            .key("special").value(nullptr)
            .key("number").value(-123.456e-7);
        });
        reader = writer.buffer;
        reader.parse([](json_reader::key_t key, const json_reader::value_t& value) {
            switch_str(key, "first", "es\"ca\"pe", "unicode", "boolean", "special", "number") {
            case_str("first"):
                assert(value.as_string() == "second");
                break;
            case_str("es\"ca\"pe"):
                assert(value.as_string() == "es\"ca\"pe");
                break;
            case_str("unicode"):
                assert(static_cast<uint8_t>(value.as_string()[0]) == 0xF0);
                assert(static_cast<uint8_t>(value.as_string()[1]) == 0x9F);
                assert(static_cast<uint8_t>(value.as_string()[2]) == 0x8C);
                assert(static_cast<uint8_t>(value.as_string()[3]) == 0x8D);
                break;
            case_str("boolean"):
                assert(value.as_boolean() == true);
                break;
            case_str("special"):
                assert(value.is_null());
                break;
            case_str("number"):
                assert(value.as_number() == -123.456e-7);
                break;
            default:
                assert(false);
                break;
            }
            return true;
        });
        assert(reader.error == nullptr);
    }
    {
        // {
        //   "aa": "bb",
        //   "cc": {
        //     "dd": {
        //       "skip": {
        //         "null": null
        //       }
        //     },
        //     "ee": {
        //       "ff": "gg"
        //     }
        //   }
        // }
        writer.object([](json_writer::object_t json) {
            json
            .key("aa").value("bb")
            .key("cc").object([](json_writer::object_t json) {
                json
                .key("dd").object([](json_writer::object_t json) {
                    json
                    .key("skip").object([](json_writer::object_t json) {
                        json
                        .key("null").value();
                    });
                })
                .key("ee").object([](json_writer::object_t json) {
                    json
                    .key("ff").value("gg");
                });
            });
        });
        reader = writer.buffer;
        reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
            switch_str(key, "aa", "cc") {
            case_str("aa"):
                assert(value.as_string() == "bb");
                break;
            case_str("cc"):
                assert(value.is_object());
                reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
                    switch_str(key, "dd", "ee") {
                    case_str("dd") :
                        assert(value.is_object());
                        return false; // skip
                    case_str("ee") :
                        assert(value.is_object());
                        reader.parse([](json_reader::key_t key, const json_reader::value_t& value) {
                            assert(key == "ff");
                            assert(value.as_string() == "gg");
                            return true;
                        });
                        break;
                    default:
                        assert(false);
                        break;
                    }
                    return true;
                });
                break;
            default:
                assert(false);
                break;
            }
            return true;
        });
        assert(reader.error == nullptr);
    }
    {
        // [
        //   {
        //     "aa": [
        //       12,
        //       // comment
        //       { "bb": "cc" },
        //       { "dd": [ 34 ] }
        //     ]
        //   }
        // ]
        writer.array([](json_writer::array_t json) {
            json
            .object([](json_writer::object_t json) {
                json
                .key("aa").array([](json_writer::array_t json) {
                    json
                    .value(12)
                    .comment("comment")
                    .object([](json_writer::object_t json) {
                        json
                        .key("bb").value("cc");
                    })
                    .object([](json_writer::object_t json) {
                        json
                        .key("dd").array([](json_writer::array_t json) {
                            json
                            .value(34);
                        });
                    });
                });
            });
        });
        reader = writer.buffer;
        reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
            assert(key.empty());
            assert(value.is_object());
            reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
                assert(key == "aa");
                assert(value.is_array());
                uint8_t index = 0;
                reader.parse([&reader, &index](json_reader::key_t key, const json_reader::value_t& value) {
                    assert(key.empty());
                    switch (index++) {
                    case 0:
                        assert(value.as_number() == 12);
                        break;
                    case 1:
                        assert(value.is_object());
                        reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
                            assert(key == "bb");
                            assert(value.as_string() == "cc");
                            return true;
                        });
                        break;
                    case 2:
                        assert(value.is_object());
                        reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
                            assert(key == "dd");
                            assert(value.is_array());
                            reader.parse([&reader](json_reader::key_t key, const json_reader::value_t& value) {
                                assert(key.empty());
                                assert(value.as_number() == 34);
                                return true;
                            });
                            return true;
                        });
                        break;
                    default:
                        assert(false);
                        break;
                    }
                    return true;
                });
                return true;
            });
            return true;
        });
        assert(reader.error == nullptr);
    }
}

void benchmark() {
    constexpr std::string_view addressbookJson =
        R"([)"
        R"(    {)"
        R"(        "name": "Alice",)"
        R"(        "id": 123,)"
        R"(        "email": "alice@example.com",)"
        R"(        "phones": [)"
        R"(            { "number": "555-1212", "type": "MOBILE" })"
        R"(        ],)"
        R"(        "ignore": { "ignore": null },)"
        R"(        "employment": {)"
        R"(            "variant": "school",)"
        R"(            "text": "MIT")"
        R"(        })"
        R"(    },)"
        R"(    {)"
        R"(        "name": "Bob",)"
        R"(        "id": 456,)"
        R"(        "email": "bob@example.com",)"
        R"(        "phones": [)"
        R"(            { "number": "555-4567", "type": "HOME" },)"
        R"(            { "number": "555-7654", "type": "WORK" })"
        R"(        ],)"
        R"(        "employment": {)"
        R"(            "variant": "unemployed")"
        R"(        })"
        R"(    })"
        R"(])";
    struct Person {
        std::string name;
        uint32_t id;
        std::string email;
        struct Phone {
            std::string number;
            std::string type;
        };
        std::vector<Phone> phones;
        struct Employment {
            std::string variant;
            std::string text;
        };
        Employment employment;
    };
    const std::initializer_list<Person> addressbookData = {
        {
            "Alice",
            123,
            "alice@example.com",
            {
                { "555-1212", "MOBILE" }
            },
            {
                "school", "MIT"
            }
        },
        {
            "Bob",
            456,
            "bob@example.com",
            {
                { "555-4567", "HOME" },
                { "555-7654", "WORK" }
            },
            {
                "unemployed", std::string()
            }
        }
    };

    volatile int32_t number = 0;
    constexpr uint32_t repsi = 100;
    constexpr uint32_t repso = 100000;
    int64_t begin_ns;
    int64_t end_ns;
    std::array<std::array<int64_t, 2>, 4> time = { {
        {{ INT64_MAX, INT64_MAX }},
        {{ INT64_MAX, INT64_MAX }},
        {{ INT64_MAX, INT64_MAX }},
        {{ INT64_MAX, INT64_MAX }}
    }};
    std::cout << "step 0/8" << std::endl;

    // =========================================================================
    //      _ ____   ___  _   _    __              __  __           _                    ____            
    //     | / ___| / _ \| \ | |  / _| ___  _ __  |  \/  | ___   __| | ___ _ __ _ __    / ___| _     _   
    //  _  | \___ \| | | |  \| | | |_ / _ \| '__| | |\/| |/ _ \ / _` |/ _ \ '__| '_ \  | |   _| |_ _| |_ 
    // | |_| |___) | |_| | |\  | |  _| (_) | |    | |  | | (_) | (_| |  __/ |  | | | | | |__|_   _|_   _|
    //  \___/|____/ \___/|_| \_| |_|  \___/|_|    |_|  |_|\___/ \__,_|\___|_|  |_| |_|  \____||_|   |_|  

    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            nlohmann::json json = nlohmann::json::parse(addressbookJson.begin(), addressbookJson.end());
            if (!json.is_array()) {
                continue;
            }
            for (const auto& it : json) {
                if (!it.is_object()) {
                    continue;
                }
                const auto itName = it.find("name");
                if (itName != it.end() && itName->is_string()) {
                    number = itName->get<std::string_view>().size();
                }
                const auto itId = it.find("id");
                if (itId != it.end() && itId->is_number()) {
                    number = itId->get<double>();
                }
                const auto itEmail = it.find("email");
                if (itEmail != it.end() && itEmail->is_string()) {
                    number = itEmail->get<std::string_view>().size();
                }
                const auto itPhones = it.find("phones");
                if (itPhones != it.end() && itPhones->is_array()) {
                    for (const auto& itPhone : itPhones.value()) {
                        if (!itPhone.is_object()) {
                            continue;
                        }
                        const auto itNumber = itPhone.find("number");
                        if (itNumber != itPhone.end() && itNumber->is_string()) {
                            number = itNumber->get<std::string_view>().size();
                        }
                        const auto itType = itPhone.find("type");
                        if (itType != itPhone.end() && itType->is_string()) {
                            number = itType->get<std::string_view>().size();
                        }
                    }
                }
                const auto itEmployment = it.find("employment");
                if (itEmployment != it.end() && itEmployment->is_object()) {
                    const auto itVariant = itEmployment->find("variant");
                    if (itVariant != itEmployment->end() && itVariant->is_string()) {
                        number = itVariant->get<std::string_view>().size();
                    }
                    const auto itText = itEmployment->find("text");
                    if (itText != itEmployment->end() && itText->is_string()) {
                        number = itText->get<std::string_view>().size();
                    }
                }
            }
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[0][0] = std::min(time[0][0], end_ns - begin_ns);
    }
    std::cout << "step 1/8" << std::endl;

    // =========================================================================

    {
    nlohmann::json json;
    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            json.clear();
            for (const auto& person : addressbookData) {
                nlohmann::json personObj;
                personObj["name"] = person.name;
                personObj["id"] = person.id;
                personObj["email"] = person.email;
                auto& phones = personObj["phones"];
                for (const auto& phone : person.phones) {
                    nlohmann::json phoneObj;
                    phoneObj["number"] = phone.number;
                    phoneObj["type"] = phone.type;
                    phones.push_back(std::move(phoneObj));
                }
                personObj["employment"]["variant"] = person.employment.variant;
                if (!person.employment.text.empty()) {
                    personObj["employment"]["text"] = person.employment.text;
                }
                json.push_back(std::move(personObj));
            }
            number = json.dump(2).size();
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[0][1] = std::min(time[0][1], end_ns - begin_ns);
    }
    }
    std::cout << "step 2/8" << std::endl;

    // =========================================================================
    //  ____             _     _     _ ____   ___  _   _ 
    // |  _ \ __ _ _ __ (_) __| |   | / ___| / _ \| \ | |
    // | |_) / _` | '_ \| |/ _` |_  | \___ \| | | |  \| |
    // |  _ < (_| | |_) | | (_| | |_| |___) | |_| | |\  |
    // |_| \_\__,_| .__/|_|\__,_|\___/|____/ \___/|_| \_|
    //            |_|                                    

    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            rapidjson::Document json;
            json.Parse(addressbookJson.data(), addressbookJson.size());
            if (!json.IsArray()) {
                continue;
            }
            for (const auto& it : json.GetArray()) {
                if (!it.IsObject()) {
                    continue;
                }
                const auto itName = it.FindMember("name");
                if (itName != it.MemberEnd() && itName->value.IsString()) {
                    number = itName->value.GetStringLength();
                }
                const auto itId = it.FindMember("id");
                if (itId != it.MemberEnd() && itId->value.IsNumber()) {
                    number = itId->value.GetDouble();
                }
                const auto itEmail = it.FindMember("email");
                if (itEmail != it.MemberEnd() && itEmail->value.IsString()) {
                    number = itEmail->value.GetStringLength();
                }
                const auto itPhones = it.FindMember("phones");
                if (itPhones != it.MemberEnd() && itPhones->value.IsArray()) {
                    for (const auto& itPhone : itPhones->value.GetArray()) {
                        if (!itPhone.IsObject()) {
                            continue;
                        }
                        const auto itNumber = itPhone.FindMember("number");
                        if (itNumber != itPhone.MemberEnd() && itNumber->value.IsString()) {
                            number = itNumber->value.GetStringLength();
                        }
                        const auto itType = itPhone.FindMember("type");
                        if (itType != itPhone.MemberEnd() && itType->value.IsString()) {
                            number = itType->value.GetStringLength();
                        }
                    }
                }
                const auto itEmployment = it.FindMember("employment");
                if (itEmployment != it.MemberEnd() && itEmployment->value.IsObject()) {
                    const auto itVariant = itEmployment->value.FindMember("variant");
                    if (itVariant != itEmployment->value.MemberEnd() && itVariant->value.IsString()) {
                        number = itVariant->value.GetStringLength();
                    }
                    const auto itText = itEmployment->value.FindMember("text");
                    if (itText != itEmployment->value.MemberEnd() && itText->value.IsString()) {
                        number = itText->value.GetStringLength();
                    }
                }
            }
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[1][0] = std::min(time[1][0], end_ns - begin_ns);
    }
    std::cout << "step 3/8" << std::endl;

    // =========================================================================

    {
    rapidjson::StringBuffer buffer;
    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            buffer.Clear();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            writer.SetIndent(' ', 2);
            writer.StartArray();
            for (const auto& person : addressbookData) {
                writer.StartObject();

                writer.String("name"); writer.String(person.name.data(), person.name.size());
                writer.String("id"); writer.Double(person.id);
                writer.String("email"); writer.String(person.email.data(), person.email.size());

                writer.String("phones");
                writer.StartArray();
                for (const auto& phone : person.phones) {
                    writer.StartObject();

                    writer.String("number"); writer.String(phone.number.data(), phone.number.size());
                    writer.String("type"); writer.String(phone.type.data(), phone.type.size());

                    writer.EndObject();
                }
                writer.EndArray();

                writer.String("employment");
                writer.StartObject();

                writer.String("variant"); writer.String(person.employment.variant.data(), person.employment.variant.size());
                if (!person.employment.text.empty()) {
                    writer.String("text"); writer.String(person.employment.text.data(), person.employment.text.size());
                }
                writer.EndObject();

                writer.EndObject();
            }
            writer.EndArray();
            number = buffer.GetSize();
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[1][1] = std::min(time[1][1], end_ns - begin_ns);
    }
    }
    std::cout << "step 4/8" << std::endl;

    // =========================================================================
    //            _       _  _                 
    //  _ __ ___ (_)_ __ (_)(_)___  ___  _ __  
    // | '_ ` _ \| | '_ \| || / __|/ _ \| '_ \ 
    // | | | | | | | | | | || \__ \ (_) | | | |
    // |_| |_| |_|_|_| |_|_|/ |___/\___/|_| |_|
    //                    |__/                 

    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            minijson::const_buffer_context json(addressbookJson.data(), addressbookJson.size());
            minijson::parse_array(json, [&](const minijson::value& value) {
                if (value.type() != minijson::Object) {
                    minijson::ignore(json);
                    return;
                }
                minijson::parse_object(json, [&](const std::string_view key, const minijson::value& value) {
                    switch_str(key, "name", "id", "email", "phones", "employment") {
                    case_str("name"):
                        if (value.type() != minijson::String) {
                            minijson::ignore(json);
                            break;
                        }
                        number = std::string_view(value.as_string()).size();
                        break;
                    case_str("id"):
                        if (value.type() != minijson::Number) {
                            minijson::ignore(json);
                            break;
                        }
                        number = value.as_double();
                        break;
                    case_str("email"):
                        if (value.type() != minijson::String) {
                            minijson::ignore(json);
                            break;
                        }
                        number = std::string_view(value.as_string()).size();
                        break;
                    case_str("phones"):
                        if (value.type() != minijson::Array) {
                            minijson::ignore(json);
                            break;
                        }
                        minijson::parse_array(json, [&](const minijson::value& value) {
                            if (value.type() != minijson::Object) {
                                minijson::ignore(json);
                                return;
                            }
                            minijson::parse_object(json, [&](const std::string_view key, const minijson::value& value) {
                                switch_str(key, "number", "type") {
                                case_str("number"):
                                    if (value.type() != minijson::String) {
                                        minijson::ignore(json);
                                        break;
                                    }
                                    number = std::string_view(value.as_string()).size();
                                    break;
                                case_str("type"):
                                    if (value.type() != minijson::String) {
                                        minijson::ignore(json);
                                        break;
                                    }
                                    number = std::string_view(value.as_string()).size();
                                    break;
                                default:
                                    minijson::ignore(json);
                                    break;
                                }
                            });
                        });
                        break;
                    case_str("employment"):
                        if (value.type() != minijson::Object) {
                            minijson::ignore(json);
                            break;
                        }
                        minijson::parse_object(json, [&](const std::string_view key, const minijson::value& value) {
                            switch_str(key, "variant", "text") {
                            case_str("variant"):
                                if (value.type() != minijson::String) {
                                    minijson::ignore(json);
                                    break;
                                }
                                number = std::string_view(value.as_string()).size();
                                break;
                            case_str("text"):
                                if (value.type() != minijson::String) {
                                    minijson::ignore(json);
                                    break;
                                }
                                number = std::string_view(value.as_string()).size();
                                break;
                            default:
                                minijson::ignore(json);
                                break;
                            }
                        });
                        break;
                    default:
                        minijson::ignore(json);
                        break;
                    }
                });
            });
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[2][0] = std::min(time[2][0], end_ns - begin_ns);
    }
    std::cout << "step 5/8" << std::endl;

    // =========================================================================

    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            std::ostringstream stream;
            minijson::writer_configuration cfg;
            minijson::array_writer json(stream,
                minijson::writer_configuration().pretty_printing(true).indent_spaces(2)
            );
            for (const auto& person : addressbookData) {
                auto personObj = json.nested_object();
                personObj.write("name", person.name);
                personObj.write("id", person.id);
                personObj.write("email", person.email);
                auto phones = personObj.nested_array("phones");
                for (const auto& phone : person.phones) {
                    auto phoneObj = phones.nested_object();
                    phoneObj.write("number", phone.number);
                    phoneObj.write("type", phone.type);
                    phoneObj.close();
                }
                phones.close();
                auto employmentObj = personObj.nested_object("employment");
                employmentObj.write("variant", person.employment.variant);
                if (!person.employment.text.empty()) {
                    employmentObj.write("text", person.employment.text);
                }
                employmentObj.close();
                personObj.close();
            }
            json.close();
            number = stream.str().size();
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[2][1] = std::min(time[2][1], end_ns - begin_ns);
    }
    std::cout << "step 6/8" << std::endl;

    // =========================================================================
    //   ____                  _ ____   ___  _   _            _ _   _                 _     ____   ___  __  __ 
    //  / ___| _     _        | / ___| / _ \| \ | | __      _(_) |_| |__   ___  _   _| |_  |  _ \ / _ \|  \/  |
    // | |   _| |_ _| |_   _  | \___ \| | | |  \| | \ \ /\ / / | __| '_ \ / _ \| | | | __| | | | | | | | |\/| |
    // | |__|_   _|_   _| | |_| |___) | |_| | |\  |  \ V  V /| | |_| | | | (_) | |_| | |_  | |_| | |_| | |  | |
    //  \____||_|   |_|    \___/|____/ \___/|_| \_|   \_/\_/ |_|\__|_| |_|\___/ \__,_|\__| |____/ \___/|_|  |_|

    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            json_reader json;
            json = addressbookJson;
            json.parse([&](json_reader::key_t key, const json_reader::value_t& value) {
                if (!value.is_object()) {
                    return false;
                }
                json.parse([&](json_reader::key_t key, const json_reader::value_t& value) {
                    switch_str(key, "name", "id", "email", "phones", "employment") {
                    case_str("name"):
                        if (!value.is_string()) {
                            return false;
                        }
                        number = value.as_string().size();
                        break;
                    case_str("id"):
                        if (!value.is_number()) {
                            return false;
                        }
                        number = value.as_number();
                        break;
                    case_str("email"):
                        if (!value.is_string()) {
                            return false;
                        }
                        number = value.as_string().size();
                        break;
                    case_str("phones"):
                        if (!value.is_array()) {
                            return false;
                        }
                        json.parse([&](json_reader::key_t key, const json_reader::value_t& value) {
                            if (!value.is_object()) {
                                return false;
                            }
                            json.parse([&](json_reader::key_t key, const json_reader::value_t& value) {
                                switch_str(key, "number", "type") {
                                case_str("number"):
                                    if (!value.is_string()) {
                                        return false;
                                    }
                                    number = value.as_string().size();
                                    break;
                                case_str("type"):
                                    if (!value.is_string()) {
                                        return false;
                                    }
                                    number = value.as_string().size();
                                    break;
                                default:
                                    return false;
                                }
                                return true;
                            });
                            return true;
                        });
                        break;
                    case_str("employment"):
                        if (!value.is_object()) {
                            return false;
                        }
                        json.parse([&](json_reader::key_t key, const json_reader::value_t& value) {
                            switch_str(key, "variant", "text") {
                            case_str("variant"):
                                if (!value.is_string()) {
                                    return false;
                                }
                                number = value.as_string().size();
                                break;
                            case_str("text"):
                                if (!value.is_string()) {
                                    return false;
                                }
                                number = value.as_string().size();
                                break;
                            default:
                                return false;
                            }
                            return true;
                        });
                        break;
                    default:
                        return false;
                    }
                    return true;
                });
                return true;
            });
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[3][0] = std::min(time[3][0], end_ns - begin_ns);
    }
    std::cout << "step 7/8" << std::endl;

    // =========================================================================

    {
    json_writer json;
    for (uint32_t i = 0; i < repso; ++i) {
        begin_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (uint32_t i = 0; i < repsi; ++i) {
            json.array([&](json_writer::array_t json) {
            for (const auto& person : addressbookData) {
                json
                .object([&](json_writer::object_t json) {
                    json
                    .key("name").value(person.name)
                    .key("id").value(person.id)
                    .key("email").value(person.email)
                    .key("phones").array([&](json_writer::array_t json) {
                    for (const auto& phone : person.phones) {
                        json.
                        object([&](json_writer::object_t json) {
                            json
                            .key("number").value(phone.number)
                            .key("type").value(phone.type);
                        });
                    }})
                    .key("employment").object([&](json_writer::object_t json) {
                        json.key("variant").value(person.employment.variant);
                        if (!person.employment.text.empty()) {
                            json.key("text").value(person.employment.text);
                        }
                    });
                });
            }});
            number = json.buffer.size();
        }
        end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        time[3][1] = std::min(time[3][1], end_ns - begin_ns);
    }
    }
    std::cout << "step 8/8" << std::endl;

    // =========================================================================

    std::cout << "nlohmann_json:        "
        << "read " << std::setw(5) << time[0][0] / 100 << " mcs "
        << std::setw(3) << (time[0][0] * 100) / time[3][0] << " %   "
        << "write " << std::setw(5) << time[0][1] / 100 << " mcs "
        << std::setw(3) << (time[0][1] * 100) / time[3][1] << " % " << std::endl;
    std::cout << "rapidjson:            "
        << "read " << std::setw(5) << time[1][0] / 100 << " mcs "
        << std::setw(3) << (time[1][0] * 100) / time[3][0] << " %   "
        << "write " << std::setw(5) << time[1][1] / 100 << " mcs "
        << std::setw(3) << (time[1][1] * 100) / time[3][1] << " % " << std::endl;
    std::cout << "minijson:             "
        << "read " << std::setw(5) << time[2][0] / 100 << " mcs "
        << std::setw(3) << (time[2][0] * 100) / time[3][0] << " %   "
        << "write " << std::setw(5) << time[2][1] / 100 << " mcs "
        << std::setw(3) << (time[2][1] * 100) / time[3][1] << " % " << std::endl;
    std::cout << "cpp_json_without_dom: "
        << "read " << std::setw(5) << time[3][0] / 100 << " mcs 100 %   "
        << "write " << std::setw(5) << time[3][1] / 100 << " mcs 100 %" << std::endl;
}

int main() {
    test();
    std::cout << "success" << std::endl;
    std::cin.get();

    benchmark();
    std::cout << "finish" << std::endl;
    std::cin.get();
    return 0;
}
