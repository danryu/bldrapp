#!/usr/bin/env python3
"""Apply MSVC compatibility patches to a mojyack-style C++ repo checkout."""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
BLDRAPP = SCRIPT_DIR.parent
PATCHES = BLDRAPP / "patches" / "msvc"

MSVC_OPT_MACROS = """
// MSVC: `return {}` in trailing-return optional methods is diagnosed as void.
#undef bail
#undef ensure
#undef unwrap
#undef unwrap_mut
#define bail(...) do { CUTIL_MACROS_PRINT_FUNC("assertion failed" __VA_OPT__(": ") __VA_ARGS__); return std::nullopt; } while(0)
#define ensure(cond, ...) do { if(!(cond)) { CUTIL_MACROS_PRINT_FUNC("assertion failed" __VA_OPT__(": ") __VA_ARGS__); return std::nullopt; } } while(0)
#define unwrap(var, opt, ...) const auto var##_o = (opt); if(!(var##_o)) { return std::nullopt; } const auto& var = *var##_o;
#define unwrap_mut(var, opt, ...) const auto var##_o = (opt); if(!(var##_o)) { return std::nullopt; } auto& var = *var##_o;

"""

WS_NDEBUG_GUARD = "#ifndef NDEBUG\n#define NDEBUG\n#endif\n#undef _DEBUG\n"


def patch_file(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if old not in text:
        raise SystemExit(f"missing {label} in {path}")
    path.write_text(text.replace(old, new), encoding="utf-8")
    print(f"patched {label} in {path}")


def inject_msvc_optional_macros(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    needle = '#include "macros/unwrap.hpp"\n'
    if needle not in text:
        raise SystemExit(f"missing unwrap include in {path}")
    if "MSVC: `return {}` in trailing-return optional methods" in text:
        print(f"skip {path}: MSVC optional macros already injected")
        return
    path.write_text(text.replace(needle, needle + MSVC_OPT_MACROS), encoding="utf-8")
    print(f"patched {path}: injected MSVC optional bail macros")


def patch_void_bodies(root: Path) -> int:
    total = 0
    for cpp in root.rglob("*.cpp"):
        text = cpp.read_text(encoding="utf-8")
        blocks: list[tuple[int, int]] = []
        i = 0
        while True:
            j = text.find("-> void", i)
            if j == -1:
                break
            k = text.find("{", j)
            if k == -1:
                break
            depth = 0
            end = k
            for pos in range(k, len(text)):
                if text[pos] == "{":
                    depth += 1
                elif text[pos] == "}":
                    depth -= 1
                    if depth == 0:
                        end = pos + 1
                        break
            blocks.append((k, end))
            i = end
        for start, end in reversed(blocks):
            chunk = text[start:end]
            chunk = (
                chunk.replace("ensure(", "ensure_v(")
                .replace("unwrap(", "unwrap_v(")
                .replace("unwrap_mut(", "unwrap_v_mut(")
            )
            text = text[:start] + chunk + text[end:]
        if blocks:
            cpp.write_text(text, encoding="utf-8")
            print(f"patched {cpp}: {len(blocks)} void bodies")
            total += len(blocks)
    return total


def patch_cutil(root: Path) -> None:
    patch_file(
        root / "src/charconv.hpp",
        "std::from_chars(str.begin(), str.end(), r, base)",
        "std::from_chars(str.data(), str.data() + str.size(), r, base)",
        "from_chars string_view iterators",
    )


def patch_cutil_macros(root: Path) -> None:
    shutil.copy2(PATCHES / "cutil-macros-assert.hpp", root / "src/assert.hpp")
    shutil.copy2(PATCHES / "cutil-macros-coop-assert.hpp", root / "src/coop-assert.hpp")
    print("installed MSVC cutil-macros patches")


def patch_crypto_utils(root: Path) -> None:
    base64 = root / "src/base64.cpp"
    text = base64.read_text(encoding="utf-8")
    if "#include <array>" not in text:
        text = text.replace("#include <vector>\n", "#include <vector>\n#include <array>\n")
        base64.write_text(text, encoding="utf-8")
        print("patched base64.cpp: +#include <array>")

    sha = root / "src/sha.cpp"
    text = sha.read_text(encoding="utf-8")
    text = text.replace(
        "    unwrap(ret, calc_generic<20>(data, \"SHA1\"));\n    return ret;",
        "    return calc_generic<20>(data, \"SHA1\");",
    )
    text = text.replace(
        "    unwrap(ret, calc_generic<32>(data, \"SHA256\"));\n    return ret;",
        "    return calc_generic<32>(data, \"SHA256\");",
    )
    text = text.replace("ensure(ctx.get() != NULL);", "if(ctx.get() == NULL) return std::nullopt;")
    text = text.replace("ensure(md != NULL);", "if(md == NULL) return std::nullopt;")
    text = text.replace(
        "ensure(EVP_DigestInit_ex(ctx.get(), md, NULL) == 1);",
        "if(EVP_DigestInit_ex(ctx.get(), md, NULL) != 1) return std::nullopt;",
    )
    text = text.replace(
        "ensure(EVP_DigestUpdate(ctx.get(), data.data(), data.size()) == 1);",
        "if(EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) return std::nullopt;",
    )
    text = text.replace(
        "ensure(EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(buf.data()), NULL) == 1);",
        "if(EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(buf.data()), NULL) != 1) return std::nullopt;",
    )
    sha.write_text(text, encoding="utf-8")
    print("patched sha.cpp")
    patch_void_bodies(root / "src")


def patch_serde(root: Path) -> None:
    # Paths resolve from libjitsimeet's jingle/serde/{xml,json}/ layout after submod clone.
    patch_file(
        root / "src/xml/format.hpp",
        '#include "util/charconv.hpp"\n#include "xml/xml.hpp"',
        '#include "../../../util/charconv.hpp"\n#include "../../../xml/xml.hpp"',
        "serde xml format includes",
    )
    patch_file(
        root / "src/json/format.hpp",
        '#include "json/json.hpp"',
        '#include "../../../json/json.hpp"',
        "serde json format include",
    )


def patch_tinyjson(root: Path) -> None:
    patch_file(
        root / "src/lexer.cpp",
        "    auto expect_string(const std::string_view expect) -> bool {\n"
        "        unwrap(str, reader.read(expect.size()));\n"
        "        return str == expect;",
        "    auto expect_string(const std::string_view expect) -> bool {\n"
        "        const auto str_o = reader.read(expect.size());\n"
        "        if(!str_o) return false;\n"
        "        const auto& str = *str_o;\n"
        "        return str == expect;",
        "lexer expect_string bool bail",
    )
    patch_file(
        root / "src/parser.cpp",
        "    auto peek() -> const Token* {\n"
        "        ensure(cursor < tokens.size());\n"
        "        return &tokens[cursor];\n"
        "    }",
        "    auto peek() -> const Token* {\n"
        "        if(!(cursor < tokens.size())) return nullptr;\n"
        "        return &tokens[cursor];\n"
        "    }",
        "json parser peek nullptr bail",
    )
    patch_file(
        root / "src/parser.cpp",
        "    template <class T>\n"
        "    auto peek_type() -> const T* {\n"
        "        unwrap(next, peek());\n"
        "        return next.get<T>();\n"
        "    }",
        "    template <class T>\n"
        "    auto peek_type() -> const T* {\n"
        "        const auto next_o = peek();\n"
        "        if(!next_o) return nullptr;\n"
        "        const auto& next = *next_o;\n"
        "        return next.get<T>();\n"
        "    }",
        "json parser peek_type nullptr bail",
    )
    patch_file(
        root / "src/parser.cpp",
        "    auto read() -> const Token* {\n"
        "        unwrap(next, peek());\n"
        "        cursor += 1;\n"
        "        return &next;\n"
        "    }",
        "    auto read() -> const Token* {\n"
        "        const auto next_o = peek();\n"
        "        if(!next_o) return nullptr;\n"
        "        const auto& next = *next_o;\n"
        "        cursor += 1;\n"
        "        return &next;\n"
        "    }",
        "json parser read nullptr bail",
    )
    patch_file(
        root / "src/parser.cpp",
        "    template <class T>\n"
        "    auto read_type() -> const T* {\n"
        "        unwrap(next, read());\n"
        "        return next.get<T>();\n"
        "    }",
        "    template <class T>\n"
        "    auto read_type() -> const T* {\n"
        "        const auto next_o = read();\n"
        "        if(!next_o) return nullptr;\n"
        "        const auto& next = *next_o;\n"
        "        return next.get<T>();\n"
        "    }",
        "json parser read_type nullptr bail",
    )
    inject_msvc_optional_macros(root / "src/lexer.cpp")
    inject_msvc_optional_macros(root / "src/parser.cpp")
    patch_void_bodies(root / "src")


def patch_tinyxml(root: Path) -> None:
    inject_msvc_optional_macros(root / "src/parser.cpp")
    patch_void_bodies(root / "src")


def patch_websocket_cpp(root: Path) -> None:
    for ws_cpp in ("impl.cpp", "client.cpp"):
        path = root / "src" / ws_cpp
        text = path.read_text(encoding="utf-8")
        if WS_NDEBUG_GUARD.strip() in text:
            print(f"skip {path}: NDEBUG guard already present")
            continue
        if "#include <libwebsockets.h>" not in text:
            raise SystemExit(f"missing libwebsockets include in {path}")
        text = text.replace(
            "#include <libwebsockets.h>",
            WS_NDEBUG_GUARD + "#include <libwebsockets.h>",
            1,
        )
        path.write_text(text, encoding="utf-8")
        print(f"patched {path}: NDEBUG guard before libwebsockets.h")
    patch_void_bodies(root / "src")


def patch_coop_meson(root: Path) -> None:
    path = root / "meson.build"
    gcc_flags = ["'-Wfatal-errors'", "'-Wno-missing-field-initializers'"]
    out: list[str] = []
    inserted = False
    for ln in path.read_text(encoding="utf-8").splitlines():
        if "add_project_arguments" in ln and any(f in ln for f in gcc_flags):
            if not inserted:
                out += [
                    "cpp_compiler = meson.get_compiler('cpp')",
                    "if cpp_compiler.get_id() == 'msvc'",
                    "  add_project_arguments(['/W3', '/DNOMINMAX'], language: 'cpp')",
                    "else",
                    "  add_project_arguments('-Wfatal-errors', language: 'cpp')",
                    "  add_project_arguments('-Wno-missing-field-initializers', language: 'cpp')",
                    "endif",
                ]
                inserted = True
            continue
        out.append(ln)
    if not inserted:
        raise SystemExit("coop meson.build: no GCC flags matched")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")
    print("patched coop meson.build")

    win_hdr = root / "include/coop/thread-event-windows.hpp"
    text = win_hdr.read_text(encoding="utf-8")
    win_hdr.write_text(text.replace("#include <unistd.h>\n\n", ""), encoding="utf-8")
    print("patched coop thread-event-windows.hpp")


def patch_gstjitsimeet_meson(root: Path) -> None:
    path = root / "meson.build"
    gcc_flags = [
        "-Wfatal-errors",
        "-fpermissive",
        "-Wno-narrowing",
        "-Wno-missing-field-initializers",
        "-std=c++23",
    ]
    out: list[str] = []
    inserted = False
    for ln in path.read_text(encoding="utf-8").splitlines():
        if "add_project_arguments" in ln and any(f"'{f}'" in ln for f in gcc_flags):
            if not inserted:
                out += [
                    "cpp_compiler = meson.get_compiler('cpp')",
                    "if cpp_compiler.get_id() == 'msvc'",
                    "  add_project_arguments(['/std:c++latest', '/permissive-', '/Zc:preprocessor', '/EHsc', '/bigobj', '/DNOMINMAX', '/D_ITERATOR_DEBUG_LEVEL=0', '/MD'], language : 'cpp')",
                    "  add_project_link_arguments('/NODEFAULTLIB:LIBCMTD', language: 'cpp')",
                    "  add_project_link_arguments('/NODEFAULTLIB:MSVCRTD', language: 'cpp')",
                    "  add_project_link_arguments('/OPT:NOREF', language: 'cpp')",
                    "else",
                    "  add_project_arguments(['-Wfatal-errors', '-fpermissive', '-Wno-narrowing', '-Wno-missing-field-initializers', '-std=c++23'], language : 'cpp')",
                    "endif",
                ]
                inserted = True
            continue
        out.append(ln)
    if not inserted:
        raise SystemExit("gstjitsimeet meson.build: no GCC flags matched")
    text = "\n".join(out) + "\n"
    text = re.sub(r"\n# examples\n.*", "\n", text, count=1, flags=re.S)
    old = "  ) + libjitsimeet_src,\n  dependencies : deps + libjitsimeet_deps,"
    new = (
        "  ) + libjitsimeet_src,\n"
        "  include_directories : include_directories('src/jitsi'),\n"
        "  dependencies : deps + libjitsimeet_deps,"
    )
    if old not in text:
        raise SystemExit("gstjitsimeet meson.build plugin stanza changed upstream")
    text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")
    print("patched gstjitsimeet meson.build")


def patch_libjitsimeet(root: Path) -> None:
    patch_file(
        root / "src/colibri.cpp",
        ".port      = ws_uri.port,",
        ".port      = static_cast<int>(ws_uri.port),",
        "colibri port cast",
    )
    patch_file(
        root / "src/jingle-handler/hostaddr.cpp",
        "auto hostname_to_addr(const char* const hostname) -> std::string {",
        "auto hostname_to_addr(const char* hostname) -> std::string {",
        "hostaddr signature match header",
    )
    for old, new, label in (
        (
            "auto cert_delete(const Cert* const cert) -> void {",
            "auto cert_delete(const Cert* cert) -> void {",
            "cert_delete signature match header",
        ),
        (
            "auto serialize_cert_der(const Cert* const cert) -> std::optional<std::vector<std::byte>> {",
            "auto serialize_cert_der(const Cert* cert) -> std::optional<std::vector<std::byte>> {",
            "serialize_cert_der signature match header",
        ),
        (
            "auto serialize_private_key_der(const Cert* const cert) -> std::optional<std::vector<std::byte>> {",
            "auto serialize_private_key_der(const Cert* cert) -> std::optional<std::vector<std::byte>> {",
            "serialize_private_key_der signature match header",
        ),
        (
            "auto serialize_private_key_pkcs8_der(const Cert* const cert) -> std::optional<std::vector<std::byte>> {",
            "auto serialize_private_key_pkcs8_der(const Cert* cert) -> std::optional<std::vector<std::byte>> {",
            "serialize_private_key_pkcs8_der signature match header",
        ),
    ):
        patch_file(root / "src/jingle-handler/cert.cpp", old, new, label)
    patch_void_bodies(root / "src")


def write_submodules_txt(path: Path, lines: list[str]) -> None:
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path}")


def patch_libjitsimeet_submodules(root: Path) -> None:
    write_submodules_txt(
        root / "submodules.txt",
        [
            "crypto-utils https://github.com/danryu/crypto-utils.git msvc",
            "cutil https://github.com/danryu/cutil.git msvc",
            "cutil-macros https://github.com/danryu/cutil-macros.git msvc",
            "serde https://github.com/danryu/serde.git msvc",
            "tinyjson https://github.com/danryu/tinyjson.git msvc",
            "tinyxml https://github.com/danryu/tinyxml.git msvc",
            "websocket-cpp https://github.com/danryu/websocket-cpp.git msvc",
        ],
    )


def patch_gstjitsimeet_submodules(root: Path) -> None:
    write_submodules_txt(
        root / "submodules.txt",
        [
            "libjitsimeet https://github.com/danryu/libjitsimeet.git msvc",
            "cutil https://github.com/danryu/cutil.git msvc",
            "cutil-macros https://github.com/danryu/cutil-macros.git msvc",
            "gstreamer-utils https://github.com/danryu/gstreamer-utils.git msvc",
        ],
    )


PATCHERS = {
    "cutil": patch_cutil,
    "cutil-macros": patch_cutil_macros,
    "crypto-utils": patch_crypto_utils,
    "serde": patch_serde,
    "tinyjson": patch_tinyjson,
    "tinyxml": patch_tinyxml,
    "websocket-cpp": patch_websocket_cpp,
    "coop": patch_coop_meson,
    "libjitsimeet": patch_libjitsimeet,
    "gstjitsimeet": patch_gstjitsimeet_meson,
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", choices=list(PATCHERS.keys()) + ["libjitsimeet-submodules", "gstjitsimeet-submodules", "gstjitsimeet-src"])
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    if not root.is_dir():
        raise SystemExit(f"not a directory: {root}")

    if args.repo in PATCHERS:
        PATCHERS[args.repo](root)
    elif args.repo == "libjitsimeet-submodules":
        patch_libjitsimeet_submodules(root)
    elif args.repo == "gstjitsimeet-submodules":
        patch_gstjitsimeet_submodules(root)
    elif args.repo == "gstjitsimeet-src":
        patch_void_bodies(root / "src")
    else:
        raise SystemExit(f"unknown repo {args.repo}")


if __name__ == "__main__":
    main()
