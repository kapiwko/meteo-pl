"""Distribution images shared by Linux builds and clean installation checks."""
TARGETS = {
    "debian": ("debian", "debian:trixie-slim"),
    "ubuntu": ("debian", "ubuntu:26.04"),
    "fedora": ("fedora", "fedora:44"),
    "arch": ("arch", None),
}
