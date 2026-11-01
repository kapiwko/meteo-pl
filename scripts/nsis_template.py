"""Keep NSIS's parsed /D directory across Craft's MultiUser initialization."""


def preserve_install_directory(template):
    marker = "Function .onInit\n    !insertmacro MULTIUSER_INIT\n"
    if template.count(marker) != 1:
        raise ValueError("Craft's NSIS initialization template changed")
    return template.replace(marker, """Var MeteoCommandLineDirectory
Function .onInit
    StrCpy $MeteoCommandLineDirectory $INSTDIR
    !insertmacro MULTIUSER_INIT
    StrCmp $MeteoCommandLineDirectory "" +2
        StrCpy $INSTDIR $MeteoCommandLineDirectory
""")
