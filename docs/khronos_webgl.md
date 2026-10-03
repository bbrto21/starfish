# Khronos WebGL Conformance Tests

The [Khronos WebGL conformance suites](https://github.com/KhronosGroup/WebGL)
are the proof of WebGL spec compliance, the way WPT is for the rest of the
platform. Starfish runs a curated subset of them against a `-DWEBGL=1` build.

The suites are version-pinned as the `third_party/webgl` submodule (shallow,
like `third_party/wpt`) and served **unmodified**: nothing in the checkout is
edited, so a pin bump is a plain submodule update and every test is exactly
what upstream ships. The three list files map onto three directories of that
one repository:

| List | Upstream directory | What it is |
|---|---|---|
| `tool/reftest/cairo/khronos_webgl.res` | `conformance-suites/1.0.3/` | Frozen WebGL 1.0.3 release |
| `tool/reftest/cairo/khronos_webgl2.res` | `conformance-suites/2.0.0/` | Frozen WebGL 2.0.0 release |
| `tool/reftest/cairo/khronos_webglsdk.res` | `sdk/tests/` | Development tip (moves with every upstream commit) |

The frozen releases still receive occasional backports upstream, so a pin
bump can change them too — re-measure all three lists, not just `sdk`.

## Running

```sh
# all three suites
xvfb-run -s '-screen 0 1920x1080x24' -a ./tool/runner/test_runner.py khronos_test
# one suite
xvfb-run -s '-screen 0 1920x1080x24' -a ./tool/runner/test_runner.py vendor_test_khronos2
# include the tracked known-failures (`#` lines) to look for newly passing tests
xvfb-run -s '-screen 0 1920x1080x24' -a ./tool/runner/test_runner.py -f vendor_test_khronos
```

`khronos_test` is deliberately not part of `reftest_all`: CI runs it in its
own job (`test_khronos_webgl` in `.github/workflows/x64_test.yml`), which
restores the submodule through `tool/ci/cache_webgl.sh` the same way the WPT
jobs restore `third_party/wpt`. `test_all` still includes it.

The runner caps the suite at 4 parallel jobs and gives each test a 1200 s
budget (`KHRONOS_WEBGL_JOBS` / `KHRONOS_WEBGL_TIMEOUT_SEC` in
`tool/runner/test_runner.py`): every job drives its own GL context, and on a
software Mesa (llvmpipe) host more of them oversubscribe the CPU until tests
that pass in isolation start timing out. A few tests legitimately take
several minutes (`uninitialized-test-2.html`, `multisample-corruption.html`).

## How results are collected

The suite's two harnesses, `js-test-pre.js` (every `conformance*/` test) and
`more/unit.js` (`conformance/more/`), report each assertion through
`window.parent.webglTestHarness`. That hook exists for Khronos's own
iframe-based runner (`webgl-conformance-tests.html`), but a page loaded at top
level has `window.parent === window`, so defining the object on the page
itself receives the same callbacks with no iframe involved.

`tool/khronos/inject_report.js` does exactly that. `tool/runner/http_server.py`
prepends it to the two harness scripts as it serves them (`--inject-script` /
`--inject-into`), so the bridge runs before the harness in the same script
and no HTML is rewritten. On the harness's `notifyFinished()` it prints one
`FAIL <message>` line per failed assertion plus a `KHRONOS DONE pass=<n>
fail=<n> skipped=<n>` summary, then ends the shell through the engine's
`testEnd()`, whose `[PASS]` / `[FAIL]` is what
`tool/drivers/basics/starfish_basic_test.py` counts. A page whose harness
never reports anything cannot pass. A skipped assertion (a case the platform
legitimately doesn't support) counts as a pass, as in Khronos's own runner.

`test_runner.py` points the server at the suite directory and sets
`TC_REPLACE_STR` so the list's repository-relative paths become
`http://localhost:11010/...` URLs; the lists themselves hold plain paths into
the submodule, so `ls` and `git log` on them work as usual.

## Lists

Same discipline as the WPT lists (`docs/wpt.md`): an uncommented line is an
expected pass and gates CI; a `#`-commented line is a tracked known failure
and is never deleted or uncommented just to make a run green. After fixing a
feature, run the list with `-f`, then uncomment the tests that now pass.

## Upgrading the pin

```sh
git -C third_party/webgl fetch --depth 1 origin <new-sha>
git -C third_party/webgl checkout <new-sha>
# every listed path must still exist (upstream renames are rare but happen)
python3 - <<'EOF'
import glob, os
for f in glob.glob("tool/reftest/cairo/khronos_webgl*.res"):
    for line in open(f):
        p = line.strip().lstrip("# ").split()
        if p and not os.path.exists(p[0].split("?")[0]):
            print(f, p[0])
EOF
xvfb-run -s '-screen 0 1920x1080x24' -a ./tool/runner/test_runner.py khronos_test   # re-measure
git add third_party/webgl tool/reftest/cairo/khronos_webgl*.res
```

Only `third_party/webgl` and the three lists change in a pin bump. The weekly
`cache_cleanup.yml` workflow republishes the CI cache for the new pin; the
first PR job after a bump builds it on a cache miss.
