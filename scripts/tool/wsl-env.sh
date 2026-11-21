#!/bin/sh

if [ -n "${VCPKG_ROOT:-}" ]; then
    return 0 2>/dev/null || true
fi

for candidate in "${PRISM_VCPKG_ROOT:-}" "${HOME:-}/vcpkg" /mnt/d/Program/App/Vcpkg/vcpkg; do
    if [ -n "${candidate}" ] && [ -x "${candidate}/vcpkg" ]; then
        VCPKG_ROOT="${candidate}"
        export VCPKG_ROOT
        case ":${PATH}:" in
            *":${candidate}:"*) ;;
            *) PATH="${candidate}:${PATH}"; export PATH ;;
        esac
        break
    fi
done

:
