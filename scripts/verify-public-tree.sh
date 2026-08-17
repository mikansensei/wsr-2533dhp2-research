#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

git diff --check
git diff --cached --check
sh -n scripts/build-selector.sh scripts/verify-public-tree.sh

bad_files=$(git ls-files | grep -E '\.(bin|itb|img|dec|dump|raw|pcap|pcapng|pem|key)$' || true)
test -z "$bad_files" || {
	echo "forbidden public artifact(s):" >&2
	echo "$bad_files" >&2
	exit 1
}

if git grep -n -E '/home/[^/]+|192\.168\.[0-9]+\.[0-9]+|BEGIN (RSA|OPENSSH|EC|PRIVATE)' -- ':!scripts/verify-public-tree.sh'; then
	echo "host path, private address, or key marker found" >&2
	exit 1
fi

oversized=$(
	git ls-files | while IFS= read -r file; do
		test "$(wc -c < "$file")" -le 1048576 || echo "$file"
	done
)
test -z "$oversized" || {
	echo "unexpected file larger than 1 MiB:" >&2
	echo "$oversized" >&2
	exit 1
}

awk -F '\t' '
  /^#/ { next }
  $4 !~ /^[0-9a-f]{64}$/ { print "bad SHA-256: " $0 > "/dev/stderr"; bad=1 }
  END { exit bad }
' data/artifact-hashes.tsv

echo "public tree checks passed"
