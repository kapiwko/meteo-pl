ARG BASE_IMAGE
FROM ${BASE_IMAGE}
USER root
# The upstream SDK preserves owner-only execute bits from the NDK archive.
# Allow the invoking user's UID to differ from the image's UID 1001.
RUN find /opt/android-sdk -type f -perm -0100 ! -perm -0001 -exec chmod a+rx {} +
USER user
WORKDIR /source
