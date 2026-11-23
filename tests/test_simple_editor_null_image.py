"""Regression check for issue #7794: the simple editor must not hand a null
InitialImage (returned by InitialImage::load for unsupported raws) to
EditorPanel::open, which dereferences it and segfaults.

This is a source-level check because the GUI cannot be exercised headless.
It requires every epanel->open (thm, ii) call that follows InitialImage::load
to be the sole body of an `if (ii) { ... }` block.
"""
import pathlib
import re
import sys

SOURCE = pathlib.Path(__file__).resolve().parent.parent / "rtgui" / "windows" / "rtwindow.cc"
LOAD = re.compile(r"InitialImage::load\s*\(\s*App::get\(\)\.argv1\(\)")
OPEN = re.compile(r"epanel->open\s*\(\s*thm,\s*ii\s*\);")
GUARDED_OPEN = re.compile(r"if\s*\(\s*ii\s*\)\s*\{\s*epanel->open\s*\(\s*thm,\s*ii\s*\);\s*\}")


def main() -> int:
    text = SOURCE.read_text(encoding="utf-8")
    load = LOAD.search(text)
    if not load:
        print("expected InitialImage::load(App::get().argv1(), ...) in rtwindow.cc")
        return 1
    after = text[load.end():]
    opens = list(OPEN.finditer(after))
    if len(opens) != 1:
        print(f"expected exactly one epanel->open (thm, ii) after InitialImage::load, found {len(opens)}")
        return 1
    guarded = list(GUARDED_OPEN.finditer(after))
    if not guarded or guarded[0].start() > opens[0].start() or guarded[0].end() < opens[0].end():
        print("InitialImage::load result is passed to epanel->open without a null check (ii)")
        return 1
    print("OK: null InitialImage is guarded before epanel->open")
    return 0


if __name__ == "__main__":
    sys.exit(main())
