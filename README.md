<div align="center">
  <img src="media/logo.png" alt="Steascii logo" width="200" height="200">
  <h1>Steascii</h1>
  <p>A portable Windows tool for preparing ASCII art for Steam.</p>
</div>

Steascii opens a text file and replaces ordinary spaces (`U+0020`) with **em spaces (`U+2003`)**, preserving the conversion used by the original Python app. An em space is different from a non-breaking space (`U+00A0`). Tabs and other Unicode characters are kept; line endings are normalized to Windows CRLF.

The native app uses C++17 and Windows' built-in controls. It runs on **Windows 10 (1607 or later) and Windows 11, x64**, without Python, a C++ redistributable installer, or administrator privileges. It works offline and never modifies your source file.

## Download and use

The existing [v1 release](https://github.com/8041q/steascii/releases/tag/v1) contains the old PyInstaller executable. The native version is built by the **Native Windows build** workflow; publishing a new GitHub release is a separate step.

After these changes are pushed to GitHub:

1. Open [Actions](https://github.com/8041q/steascii/actions) and select **Native Windows build**.
2. Open a successful run, or choose **Run workflow** to start a manual build.
3. Download the **Steascii-windows-x64** artifact at the bottom of the run. GitHub requires sign-in to download Actions artifacts.
4. Extract the ZIP and run `Steascii.exe`. You only need the executable; `SHA256SUMS.txt` is provided to verify its integrity.
5. Click **Open text file...**, choose your file, then click **Copy result** and paste the converted text wherever you need it.

The preview uses a monospaced font with horizontal and vertical scrolling and no word wrapping. You can open another file in the same window. Cancelling the picker or encountering a file error keeps the previous result. Copying an empty file places empty text on the clipboard. A busy clipboard produces an error and can be retried.

Files up to **32 MiB** are supported:

- UTF-8, with or without a byte-order mark (BOM).
- UTF-16 little-endian or big-endian, with a BOM.
- Unmarked non-UTF-8 text using the PC's active Windows ANSI code page. Save as UTF-8 for consistent results across PCs.

Malformed BOM-marked Unicode, UTF-32, and embedded null characters are rejected. Valid unmarked UTF-8 is always preferred over ANSI.

To verify an extracted executable in PowerShell:

```powershell
Get-FileHash .\Steascii.exe -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

### Windows security warnings

The native executable is unsigned. Replacing the Python bundle removes PyInstaller packaging, but does **not** guarantee that Defender, SmartScreen, or Smart App Control will accept every build. Do not disable Windows security to use the app.

The Windows workflow checks DLL dependencies, updates Microsoft Defender definitions, and scans its executable before uploading it. If the scan cannot run or detects a threat, the workflow fails. That scan applies to that particular build and does not replace testing the actual download on a clean Windows PC.

For a named antivirus detection, submit the exact flagged binary through [Microsoft Security Intelligence](https://www.microsoft.com/en-us/wdsi/filesubmission), selecting **Software developer** and supplying the detection name. An unrecognized-app/unknown-publisher warning is a separate reputation issue; see [Microsoft's SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).

## Build on Windows

Developers need CMake 3.20 or newer and Visual Studio 2022 Build Tools with **Desktop development with C++** and the Windows SDK. Users of the resulting executable need none of these tools.

Run from the project directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --timeout 30
```

The executable is `build\Release\Steascii.exe`. The MSVC runtime is statically linked (`/MT`) and the only DLL dependencies are supplied by Windows. The icon and an `asInvoker`/DPI-aware manifest are embedded in the executable.

The GitHub workflow runs on pull requests, pushes to `master`, and manual dispatches. It builds the x64 Release executable, runs portable and Windows integration tests, inspects DLL imports, scans with Defender, and uploads the executable plus its SHA-256 checksum. It does not publish or replace a release automatically.

Before distributing a new build, complete the [Windows smoke test](docs/windows-smoke-test.md) on a Windows PC without development tools installed. Upload the verified executable and checksum as assets on a new release; retain the old v1 release for reference.

## Test the conversion on macOS or Linux

The conversion and Unicode decoding code is separate from the Windows interface. With a C++17 compiler and CMake installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On these platforms CMake builds the portable tests only. The Windows tests additionally exercise actual file loading, Unicode filenames, size limits, Windows ANSI decoding, and Unicode clipboard operations, including a busy clipboard and retry.

## Original Python version

`Steascii.py` remains available for anyone who wants to run or inspect the original program. That version requires Python with Tkinter and retains its console workflow. The native executable does not use it.

## Preparing ASCII art for Steam

For background, see [How to Make ASCII Art for the Custom Info Box on Your Steam Profile Page](https://steamcommunity.com/sharedfiles/filedetails/?id=2235568594).

1. Convert an image to ASCII using [Ascii Generator 2](https://ascgendotnet.jmsoftware.co.uk/). Drag in the image, adjust its size, contrast and brightness, and copy the resulting art to a text file.
2. Open the text file in Steascii and click **Copy result**.
3. If your Steam layout requires full-width characters, paste into [Dencode's character-width converter](https://dencode.com/en/string/character-width), select **Full Width**, and copy the result. This is an optional external step; Steascii itself only converts spaces.
4. Preview in a text editor using a monospaced font and paste into your Steam profile.

![ASCII Generator size option](media/size.png)

## Acknowledgments

- [Best-README-Template](https://github.com/othneildrew/Best-README-Template)
- [Steam ASCII art guide](https://steamcommunity.com/sharedfiles/filedetails/?id=2235568594)
