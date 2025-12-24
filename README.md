# u3-build (payload)

DLL payload that is downloaded and injected by the loader.

## Requirements
- Windows 10/11
- Visual Studio Build Tools 2022 (Desktop C++)
- CMake 3.20+

## Build (x86 example)
```
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Output: `build/Release/payload.dll`

## Deploy
- Copy the DLL to the server:
  - Default: `server/data/payload.dll`
  - Per product: update `products[].payload_path` in `server/config.json`
- Restart the server or wait for the next request; it reads payload from disk.

## Notes
- The payload reads runtime config from shared memory written by the loader.
- The payload bitness must match the target process.
