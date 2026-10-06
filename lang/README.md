# Translation files

Translations are plain UTF-8 text so they can be edited without knowing Lua or C++.

Format:

```text
# comments begin with #
key=value
```

Rules:

- file name is the language code (`en.txt`, `vi.txt`, `zh_CN.txt`),
- keys must match `lang/en.txt`, which is the fallback language,
- values are UTF-8,
- printf-style placeholders such as `%d` and `%s` must be preserved,
- application pages call `shell.i18n.t("key", ...)` instead of hardcoding visible UI strings.

The Lua simulator watches `lang/*.txt`; saving a translation file reloads the Lua VM transactionally
without restarting the native app or simulator window.
