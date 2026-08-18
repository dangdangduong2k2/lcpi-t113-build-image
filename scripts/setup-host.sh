#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
	echo "Run this script inside Ubuntu/WSL2, not in PowerShell." >&2
	exit 1
fi

sudo apt-get update
sudo apt-get install -y \
	bc \
	build-essential \
	bzip2 \
	ca-certificates \
	cpio \
	diffutils \
	file \
	findutils \
	git \
	gzip \
	libncurses-dev \
	patch \
	perl \
	python3 \
	rsync \
	sed \
	unzip \
	wget \
	which \
	xz-utils

echo "Host dependencies are ready. Next: make configure && make build"
