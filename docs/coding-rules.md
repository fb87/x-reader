# Coding Rules

## Language

Use C++17 syntax only as a namespacing layer over procedural C-style code.

Allowed:

- Namespaces
- Structs and enums
- Free functions
- Fixed-width integer types
- C arrays and pointers
- Explicit allocation and release
- `static` internal functions
- `extern "C"` for ESP-IDF entry points and C callbacks

Forbidden:

- Classes and inheritance
- Templates
- STL containers and algorithms
- `std::string`
- Exceptions
- RTTI
- Smart pointers
- Lambdas
- Operator overloading
- Virtual functions
- Global objects with constructors
- Hidden allocation or ownership

## Naming

Use snake_case for all project identifiers:

- Files: `text_layout.cpp`
- Namespaces: `xreader::text_layout`
- Functions: `parse_epub()`
- Variables and fields: `page_index`
- Structs: `framebuffer_t`
- Enums: `refresh_mode_t`
- Enum values: `refresh_full`

Use uppercase names only for preprocessor macros required by the toolchain or
include guards.

## Memory and Errors

- Return `bool` or an explicit error enum for fallible operations.
- Check every IDF return value.
- Keep ownership visible in function signatures and structs.
- Use fixed-size buffers when protocol limits allow it.
- Validate lengths before arithmetic or copying.
- Never use unbounded string functions on book or XML data.
- Log enough context to diagnose a hardware or book failure.

## Formatting

`.clang-format` is authoritative for layout. Run:

```sh
clang-format -i path/to/file.cpp path/to/file.hpp
```

The project uses four-space indentation, Allman braces, left pointer
alignment, and a 100-column limit.

## Includes

- Include the matching header first in implementation files.
- Prefer local project headers over transitive includes.
- Keep IDF headers in the module that directly uses them.
- Do not introduce a compatibility wrapper for an API without a concrete
  portability requirement.

## Comments

Comments should explain hardware timing, file-format rules, ownership, or a
non-obvious algorithm. Do not comment obvious assignments or restate code.
