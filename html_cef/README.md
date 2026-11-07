# Windows CEF backend

This experimental backend uses the Windows x64 CEF binary distribution
`156.0.26+g8ea0c73+chromium-156.0.8078.88`. The distribution is supplied
separately and is not included in this repository.

Configure and build with CMake 3.21 or newer and Visual Studio 2022:

```powershell
cmake -S html_cef -B build-html-cef -G "Visual Studio 17 2022" -A x64 -DCEF_ROOT=D:/path/to/cef_binary_windows64
cmake --build build-html-cef --config Release --parallel 12 --target html_cef html_cef_helper
```

Place `html_cef.dll` and `html_cef_helper.exe` in the game's `hl2sb/bin`
directory. The helper must be able to load the CEF runtime from that directory.
For the SDK version above, copy the SDK `Release` DLLs,
`v8_context_snapshot.bin`, and `vk_swiftshader_icd.json`, plus the SDK
`Resources` files and `locales` directory, alongside the helper. The caller
must pass the absolute helper path and an absolute writable cache directory
when creating the backend. Do not mix files from different CEF releases.

An independent Windows x64 smoke test has exercised an `asset://` page,
pre-document JavaScript registration, two callback requests and replies,
software-rendered pixels, and shutdown. This is not a substitute for game or
platform testing.

Known limitations:

- The current build disables the CEF sandbox. Treat external pages as unsafe;
  this configuration is not a finished security design.
- Navigation is limited to `http`, `https`, `asset`, `data`, and `about:blank`.
  The asynchronous navigation-policy event is not implemented.
- Browser focus loss, complete key mapping, IME behavior, popup windows and
  cursor changes have not been verified in game.
- Callbacks queued by a previous document and registrations across a
  cross-site renderer process transition need further lifecycle validation.
- Document-ready uses a renderer-side DOMContentLoaded notification. Loading
  completion waits up to two seconds for that notification; the exact event
  order still needs comparison with the reference browser.
- Asset and callback payloads use the current ABI and need broader testing,
  including cancellation, malformed content and multiple simultaneous views.
