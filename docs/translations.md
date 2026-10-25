# Translations

The source language is English. Polish UI text belongs in
[`po/pl/meteo-pl.po`](../po/pl/meteo-pl.po), using gettext and KDE KI18n, as in
btrfs-backup. The application and Plasma widget share the `meteo-pl` domain.

QML uses `I18n.i18n("English source text")`; C++ uses
`Meteo::i18n("English source text")`. Both use the same catalog. Translate complete
sentences and substitute dynamic values with `%1`, `%2`, etc. Use `i18np` for
counts, and add translator comments where a weather term is ambiguous. Keep
place names, user-created profile names, protocol keys and URLs untranslated.

After changing source messages, install GNU gettext and run:

```bash
python3 scripts/update-translations.py
```

This updates `po/meteo-pl.pot`, merges each language catalog and checks syntax
and format placeholders. Edit `.po` files with Lokalize, Poedit or a text editor.
There must be no untranslated or fuzzy entries in the Polish catalog.

To add a language, create `po/<language>/meteo-pl.po` from the template, set its
language and plural rules, then configure and rebuild. CMake discovers catalogs,
compiles them with `msgfmt --check --check-format`, installs them under
`share/locale` and embeds them in the shared module. KI18n uses a private temporary
copy of the embedded catalogs, so translation also works from a build directory,
in relocatable Windows/macOS bundles, on Android and inside Plasma without
changing the host application's translation domain. Temporary files are removed
when the process exits.

The system language selects the UI language. An English fallback is always
available. On Linux you can preview the two bundled languages with:

```bash
LANGUAGE=en_US ./build/dev/apps/meteo/meteo-pl
LANGUAGE=pl_PL ./build/dev/apps/meteo/meteo-pl
```

Dates and number formatting follow the system locale; `LANGUAGE` changes message
translation only. To preview both together, set a matching installed `LC_ALL`
locale as well. GNU gettext intentionally disables translations in the `C` and
`C.UTF-8` locales; use an installed UTF-8 locale such as `en_US.UTF-8` or
`pl_PL.UTF-8` when testing language overrides.

Run the translation tests with:

```bash
ctest --test-dir build/dev -R translation_ --output-on-failure
```

They check native errors, parameter names, placeholders, Polish plural forms,
independent QML engines and preservation of the host's translation domain. The
normal presentation tests use English, while the translation tests run in fresh
processes for English and Polish.

The application desktop entry, notification descriptions and Plasma metadata
use their standard localized fields (`Name[pl]`, `Comment[pl]`, etc.). Editorial
content and original map images are provided by meteo.pl and keep their original
language.
