#!/bin/bash

set -e

if [ "$MESON_SOURCE_ROOT" ]; then
    cd "$MESON_SOURCE_ROOT"
else
    cd "$(dirname $0)"
fi

if [ -z "$MESONREWRITE" ]; then
    MESONREWRITE="meson rewrite"
fi

case "$1" in
    print-version)
        desc=$(git describe --exclude 'xfce-[0-9].*' --dirty=+ 2>/dev/null || true)

        if [ -z "$desc" ]; then
            gitrev=$(git rev-parse --short HEAD 2>/dev/null || echo "UNKNOWN")
            version="0.1.0git-${gitrev}"
        elif [[ $desc =~ ([0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?)-[0-9]+-g([0-9a-f]+\+?)$ ]]; then
            version="${BASH_REMATCH[1]}git-${BASH_REMATCH[3]}"
        elif [[ $desc =~ ([0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?\+?)$ ]]; then
            version="${BASH_REMATCH[1]}"
        else
            echo "Unable to parse 'git describe' output" >&2
            exit 1
        fi
        ;;

    rewrite-version)
        $MESONREWRITE --sourcedir="$MESON_PROJECT_DIST_ROOT" kwargs set project / version "$2"
        ;;

    *)
        echo "Invalid option: $1" >&2
        echo >&2
        echo "Usage:" >&2
        echo "    $0 print-version" >&2
        echo "    $0 rewrite-version NEW_VERSION" >&2
        exit 1
        ;;
esac

echo "$version"
