#!/usr/bin/env bash

set -e
set -o pipefail
set -u

function step { >&2 echo -e "\033[1m\033[36m* $@\033[0m"; }
function finish { >&2 echo -en "\033[0m"; }
trap finish EXIT

VERSION_TAG=android-v10.0.2-pre.1
step "Checking out source code at ${VERSION_TAG}"
git checkout ${VERSION_TAG}
