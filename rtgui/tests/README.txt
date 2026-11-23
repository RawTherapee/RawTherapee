White balance feedback regression tests

Configure with -DWITH_WB_FEEDBACK_TESTS=ON (default OFF), for example:

  cmake --preset dev -B build/wb-feedback -DWITH_WB_FEEDBACK_TESTS=ON
  nice -n 10 cmake --build build/wb-feedback --target rth rth-cli wb-feedback-test -j4
  ctest --test-dir build/wb-feedback -R '^wb-feedback$' --output-on-failure

The test requires a GTK display. It returns skip code 77 without one. On a
headless host use Xvfb, or start a private GTK 3 Broadway daemon and supply
GDK_BACKEND=broadway and its BROADWAY_DISPLAY when running CTest. A skipped
run does not validate the GUI behavior.

The executable creates and removes its own settings/cache directories and
small PNG input. It does not require a camera RAW or modify user preferences.
GUI tests explicitly queue feedback before editing and then dispatch the idle
queue, keeping normal adjuster notifications pending. Worker tests use a
condition-variable barrier to submit a new revision after the older input snapshot is copied but before its white
balance result is computed. No processing sleeps are needed to reproduce the ordering.

The optional target compiles the production GUI sources with a test entry
point and uses the same engine and GUI dependencies as the application.
