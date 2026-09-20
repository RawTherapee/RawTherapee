# this deliberately has no shebang (#!) line.
# See LibRaw.cmake's add_custom_command(), which should
# execute it with a POSIX-compliant shell.
# It expects exactly two arguments, which will be mapped to
# MAKE and LOGICAL_PROCESSORS

if [ 2 -gt $# ] ; then
    echo >&2 "Usage: $0 <MAKE_CMD> <LOGICAL_PROCESSORS> [MAKEFLAGS [...]]"
    exit 1
fi

_make="$1"
LOGICAL_PROCESSORS="$2"
shift
shift

_gmake="$(set +e; type >/dev/null gmake && echo gmake)"  # gmake if on PATH, else empty
_make_arg=
case "${_make##*/}" in
    gmake|make) printf '%s\n' >&2 "-- ${0##*/} is using inherited $_make"
        ;;
    *)  _make="${_gmake:-make}"
        case "$MAKEFLAGS" in
            *--jobserver-auth*) ;;
            *)                  _make_arg="-j${LOGICAL_PROCESSORS}" ;;
        esac
        printf '%s\n' >&2 "-- ${0##*/} is using generated $_make $_make_arg"
        ;;
esac
if [ -n "${_make_arg}" ] ; then
    exec "${_make}" "${_make_arg}" "$@"
else
    exec "${_make}" "$@"
fi
