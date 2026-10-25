#!/usr/bin/env python3
"""Extract English source messages and merge every gettext catalog."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    os.chdir(ROOT)
    with tempfile.TemporaryDirectory(prefix="meteo-gettext-") as temporary:
        catalogs = []
        for name, language, extensions in [
            ("cpp", "C++", {".cpp", ".hpp"}),
            ("qml", "JavaScript", {".qml", ".js"}),
        ]:
            files = sorted(str(path) for root in ["src", "apps", "integrations"]
                           for path in Path(root).rglob("*") if path.suffix in extensions)
            catalog = str(Path(temporary) / (name + ".pot"))
            subprocess.run(["xgettext", "--from-code=UTF-8", "--language=" + language,
                            "--keyword=i18n:1", "--keyword=i18np:1,2",
                            "--flag=i18n:1:kde-format", "--flag=i18np:1:kde-format", "--flag=i18np:2:kde-format",
                            "--package-name=meteo-pl", "--copyright-holder=Kamil Piwowarski",
                            "--msgid-bugs-address=https://github.com/kapiwko/meteo-pl/issues",
                            "--add-comments=TRANSLATORS", "--sort-output", "--output=" + catalog, *files], check=True)
            catalogs.append(catalog)
        subprocess.run(["msgcat", "--use-first", "--sort-output", "--output-file=po/meteo-pl.pot", *catalogs], check=True)
        for catalog in sorted(Path("po").glob("*/meteo-pl.po")):
            subprocess.run(["msgmerge", "--update", "--backup=none", "--no-fuzzy-matching", str(catalog), "po/meteo-pl.pot"], check=True)
            subprocess.run(["msgfmt", "--check", "--check-format", "--output-file=" + os.devnull, str(catalog)], check=True)


if __name__ == "__main__":
    main()
