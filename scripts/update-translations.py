#!/usr/bin/env python3
"""Extract English source messages and merge every gettext catalog."""
import os
import json
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
        profile_messages = {}
        def collect_labels(value, path):
            if isinstance(value, dict):
                for key, child in value.items():
                    if key in ("name", "title", "label") and isinstance(child, str):
                        profile_messages.setdefault(child, set()).add(str(path))
                    else:
                        collect_labels(child, path)
            elif isinstance(value, list):
                for child in value:
                    collect_labels(child, path)
        for directory in ("resources/profiles", "resources/charts"):
            for path in sorted(Path(directory).glob("*.json")):
                collect_labels(json.loads(path.read_text()), path)
        profile_catalog = Path(temporary) / "profiles.pot"
        profile_catalog.write_text('msgid ""\nmsgstr ""\n"Content-Type: text/plain; charset=UTF-8\\n"\n\n'
            + "\n\n".join("#: " + " ".join(sorted(paths)) + "\nmsgid " + json.dumps(message, ensure_ascii=False)
                           + '\nmsgstr ""' for message, paths in sorted(profile_messages.items())))
        catalogs.append(str(profile_catalog))
        subprocess.run(["msgcat", "--use-first", "--sort-output", "--output-file=po/meteo-pl.pot", *catalogs], check=True)
        for catalog in sorted(Path("po").glob("*/meteo-pl.po")):
            subprocess.run(["msgmerge", "--update", "--backup=none", "--no-fuzzy-matching", str(catalog), "po/meteo-pl.pot"], check=True)
            subprocess.run(["msgfmt", "--check", "--check-format", "--output-file=" + os.devnull, str(catalog)], check=True)


if __name__ == "__main__":
    main()
