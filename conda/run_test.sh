#!/usr/bin/env bash

set -x

if [[ "$CONDA_TOOLCHAIN_HOST" != "$CONDA_TOOLCHAIN_BUILD" ]]; then
  echo -e "Drop testing when cross-compiling..." && exit 0
fi

echo -e "\033[1;34m--- START ---\033[0m"

if "$PKG_NAME" --help; then  # note! abseil flags returns 1
  (>&2 echo -e "\033[1;31mERROR: Unexpected error code.\033[0m") && exit 1
fi


if [ -f "$PKG_NAME-benchmark" ]; then
  if ! "$PKG_NAME-benchmark" --help; then  # note! benchmark returns 0
    (>&2 echo -e "\033[1;31mERROR: Unexpected error code.\033[0m") && exit 1
  fi
fi

echo -e "\033[1;34m--- DONE ---\033[0m"
