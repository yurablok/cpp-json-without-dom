#!/usr/bin/python3
import os

if not os.path.exists("deps"):
    os.mkdir("deps")
os.chdir("deps")

os.system("git clone --branch main --single-branch https://github.com/yurablok/cpp-adaptive-benchmark.git")

os.system("git clone --branch 1.0 --single-branch https://github.com/giacomodrago/minijson_reader.git")

os.system("git clone --branch master --single-branch https://github.com/giacomodrago/minijson_writer.git")

os.system("git clone --branch v3.10.5 --single-branch https://github.com/nlohmann/json.git nlohmann_json")

os.system("git clone --branch master --single-branch https://github.com/Tencent/rapidjson.git")
os.chdir("rapidjson")
os.system("git checkout --quiet 232389d4f1012dddec4ef84861face2d2ba85709")
os.chdir("..")

os.system("git clone --branch master --single-branch https://github.com/martinmoene/string-view-lite")
os.chdir("string-view-lite")
os.system("git checkout --quiet f7aca36f5caa05e451f6887aa707df89197e6de6")
os.chdir("..")

os.system("git clone --branch main --single-branch https://github.com/yurablok/switch-str.git")

os.system("git clone --branch master --single-branch https://github.com/mpark/variant")
os.chdir("variant")
os.system("git checkout --quiet 23cb94f027d4ef33bf48133acc2695c7e5c6f1e7")
os.chdir("..")
