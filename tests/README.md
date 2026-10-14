# Tests

    make test              build and run everything
    make test TESTS=net    just tests/test_net.c

Each `tests/test_*.c` is its own program, linked against the **real** objects
in `build-$(CPU)/`, so a test exercises shipped code rather than a copy.

## Why this works at all

q2admin gets everything from the engine and the wrapped mod through plain
globals, so a test supplies its own:

| global | what it is | how to fake it |
|---|---|---|
| `gi` | the engine's function table | a struct of function pointers - assign the fields you need |
| `proxyinfo` | per-client state | point it at your own array |
| `maxclients` | `cvar_t *` | point it at your own `cvar_t` |
| `ltime` | server clock, seconds | set it |

`tests/stub_engine.c` does all of that. Call `stub_engine_init()` first.

## Mocking a client

A client is a few fields in a `proxyinfo` slot. No engine, no network, no map:

```c
stub_add_client(1, "bob", "q2pro r1892", PROTOCOL_R1Q2);
checkProtocolDowngrade(1);
CHECK(stub_signals_for(1) & SIGNAL_PROTOCOL_DOWNGRADE);
```

`tests/test_protocol.c` is the worked example. Copy it.

## What is cheap and what is not

- **Cheap:** anything not touching `gi` - `g_net.c`, `g_regex.c`, `g_json.c`,
  most of `g_util.c`.
- **Cheap, needs the stub:** code reading or writing `proxyinfo`, like
  `g_protocol.c`.
- **More work:** `g_signal.c` and `g_log.c` need `logEvent()`,
  `addCmdQueue()` and a fuller `gi`. Worth doing - `convertToLogLine()` is a
  pure string formatter where a test would have caught a real truncation bug.
- **Not worth faking:** `ClientThink()`, the Cloud Admin socket code, and
  anything calling `ge_mod->*`. Those need a live engine and a real mod; test
  them on a server.

## Gotcha

Include `g_local.h` and use the real `proxyinfo_t`. Do not declare a smaller
version of the struct in a test - the field offsets then disagree with the
compiled code under test, which reads the wrong members and passes or fails
for the wrong reason. This happened while writing `test_protocol.c`.
