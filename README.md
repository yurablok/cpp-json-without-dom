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
json.parse([&json](json_reader::key_t key, const json_reader::value_t& value) {
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
        json.parse([](uint32_t index, const json_reader::value_t& value) {
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

| Tool                                         |       Encoding        |
|:---------------------------------------------|:---------------------:|
| [JSON for Modern C++][1]                     |         UTF-8         |
| [RapidJSON][2]                               | UTF-8, UTF-16, UTF-32 |
| [minijson_reader][3]<br>[minijson_writer][4] |     UTF-8, UTF-16?    |
|  C++ JSON without DOM                        |         UTF-8         |

`MSVC 14.34, Vermeer, 4.62 GHz`
| Tool                                         | Read bench |   %   | Write bench |   %   |
|:---------------------------------------------|-----------:|------:|------------:|------:|
| [JSON for Modern C++][1]                     |  7us 042ns | 477.2 |   4us 470ns | 588.1 |
| [RapidJSON][2]                               |  2us 309ns | 156.4 | 760ns 031ps |100<sub>1</sub>|
| [minijson_reader][3]<br>[minijson_writer][4] |  1us 914ns | 129.7 |   5us 858ns |770.8<sub>2</sub>|
|  C++ JSON without DOM                        |  1us 475ns |   100 | 857ns 924ps | 112.8 |

`MSVC 14.35, Cortex-X1, 2.91 GHz`
| Tool                                         | Read bench |   %   | Write bench |   %   |
|:---------------------------------------------|-----------:|------:|------------:|------:|
| [JSON for Modern C++][1]                     | 13us 497ns | 684.1 |   8us 759ns | 757.2 |
| [RapidJSON][2]                               |  8us 579ns |434.8<sub>3</sub>|1us 156ns|100<sub>1</sub>|
| [minijson_reader][3]<br>[minijson_writer][4] |  3us 096ns | 156.9 |  14us 041ns |1213.9<sub>2</sub>|
| C++ JSON without DOM                         |  1us 972ns |   100 |   1us 222ns | 105.6 |

1. SAX style API that requires an explicit call to object/array begin/end.
2. Due to slow `std::ostringstream`.
3. Strange high kernel times.

You can see benchmarks of other libs here
[miloyip/nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark#parsing-time).

[1]: https://github.com/nlohmann/json
[2]: https://github.com/Tencent/rapidjson
[3]: https://github.com/giacomodrago/minijson_reader
[4]: https://github.com/giacomodrago/minijson_writer
