# C++ JSON without DOM

C++17(11`*`) callback-based DOM-less parsing and generating JSON messages.

### Applicability:

What are typical scenarios of working with JSON? Parsing JSON messages into
custom data structures with checking for errors and field availability, and
generating JSON messages from custom data structures. But storing data directly
in JSON containers is not a good idea. Therefore, we can work with JSON messages
more efficiently without using intermediate storage of its DOM structure due to
a simplicity of the format.

### Benefits and features:

- fast (see comparison)
- minimal API, single header ~1000 LOC
- one-pass parser without intermediate DOM representation 
- zero-copy parse if no escape (`\`)
- single-line comments (`// ...`)
- single-line branches (`{ [ { } ] }`)
- `*` C++11 support by using of third-party libs
  ([string_view](https://github.com/martinmoene/string-view-lite)
  and [variant](https://github.com/mpark/variant))

### Basic example:

```jsonc
{
  "number": 123,
  "array": [
    "string",
    // comment
    null,
    {}
  ]
}
```

```cpp
json_writer json;
json.object([](json_writer::object_t json) {
    json
    .key("number").value(123)
    .key("array").array([](json_writer::array_t json) {
        json
        .value("string")
        .comment(" comment")
        .value(nullptr)
        .object(nullptr);
    });
});
std::ofstream("file.json") << json.buffer;
```

For faster key matching, it is recommended to use
[switch-str](https://github.com/yurablok/switch-str.git).

```cpp
json_reader json;
json = string_to_parse;
assert(json.is_object());
json.parse([&json](json_parse::key_t key, const json_parse::value_t& value) {
    switch_str(key, "number", "array") {
    case_str("number"):
        if (!value.is_number()) {
            return;
        }
        assert(value.as_number() == 123);
        break;
    case_str("array"): {
        if (!value.is_array()) {
            return;
        }
        json.parse([](uint32_t index, const json_parse::value_t& value) {
            switch (index) {
            case 0:
                assert(value.is_string());
                break;
            case 1:
                assert(value.is_null());
                break;
            case 2:
                assert(value.is_object());
                break;
            default:
                break;
            }
        });
    }
    default:
        break;
    }
});
assert(json.error == nullptr);
```

### Comparison with alternatives:

|                             Tool                                   | Read bench, % | Write bench, % |       Encoding        |
| :----------------------------------------------------------------: | :-----------: | :------------: | :-------------------: |
| [JSON for Modern C++](https://github.com/nlohmann/json)            |      502      |       497      |         UTF-8         |
| [RapidJSON](https://github.com/Tencent/rapidjson)                  |      136      | 82<sub>1</sub> | UTF-8, UTF-16, UTF-32 |
| [minijson_reader](https://github.com/giacomodrago/minijson_reader) |      121      |                |     UTF-8, UTF-16?    |
| [minijson_writer](https://github.com/giacomodrago/minijson_writer) |               |641<sub>2</sub> |     UTF-8, UTF-16?    |
|  C++ JSON without DOM                                              |      100      |       100      |         UTF-8         |

1. SAX style API that requires an explicit call to object/array begin/end.
2. Due to slow `std::ostringstream`.

You can see benchmarks of other libs here
[miloyip/nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark#parsing-time).
