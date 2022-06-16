#!/bin/bash
set -euox

TEST_TMPDIR=$(mktemp -d)
trap "rm -rf ${TEST_TMPDIR};" err exit

PRIMARY_MIRRORS=" http://www.slackel.gr/repo/x86_64/slackware-current/ https://slackware.uk/slackware/slackware64-current/ https://slackware.osuosl.org/slackware64-current/"
for MIRROR in ${PRIMARY_MIRRORS}; do
    curl -s -I ${MIRROR}/GPG-KEY &>/dev/null && break
done

slaptget="${1}"
config=${TEST_TMPDIR}/config
cat > ${config} << EOF
WORKINGDIR=${TEST_TMPDIR}/slapt-get-cache
EXCLUDE=
SOURCE=${MIRROR}:OFFICIAL
SOURCE=https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/:OFFICIAL
EOF
mkdir -p ${TEST_TMPDIR}/var/lib/pkgtools/packages
cat > ${TEST_TMPDIR}/var/lib/pkgtools/packages/aaa_base-14.2-x86_64-7 << EOF
PACKAGE NAME:     aaa_base-14.2-x86_64-7
COMPRESSED PACKAGE SIZE:     12K
UNCOMPRESSED PACKAGE SIZE:     90K
PACKAGE LOCATION: /var/cache/slapt-get/./slackware64/a/aaa_base-14.2-x86_64-7.txz
PACKAGE DESCRIPTION:
aaa_base: aaa_base (Basic Linux filesystem package)
aaa_base:
aaa_base: Sets up the empty directory tree for Slackware and adds an email to
aaa_base: root's mailbox welcoming them to Linux. :)  This package should be
aaa_base: installed first, and never uninstalled.
aaa_base:
aaa_base:
aaa_base:
aaa_base:
aaa_base:
aaa_base:
FILE LIST:
./
EOF

ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --add-keys
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --update
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --upgrade -s
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --upgrade --print-uris
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --dist-upgrade -s
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --remove aaa_base -s
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --search slapt
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --installed
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --available
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --remove --remove-obsolete -s
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --install-set xap -s
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --install aaa_base --reinstall --download-only
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --autoclean
ROOT=${TEST_TMPDIR} ${slaptget} --config "${config}" --clean
find ${TEST_TMPDIR}
