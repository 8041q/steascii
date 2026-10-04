# Windows release smoke test

Run these checks on the exact executable that will be distributed, on a Windows 10 (1607+) or Windows 11 x64 PC without Python, Visual Studio, or additional runtime installers. This is a manual GUI checklist, separate from the automated conversion and file/clipboard tests.

1. Download/extract the build and verify its SHA-256 checksum. Scan the executable with updated Microsoft Defender. Record the Windows version, build hash, scan result, and any SmartScreen/Smart App Control warning. Do not treat unsigned-app reputation warnings as proof of malware or bypass a named detection without review.
2. Launch `Steascii.exe` as a normal user. Confirm the logo appears, no terminal or elevation prompt opens, and **Copy result** is initially disabled.
3. Open a UTF-8 text file containing leading/trailing/repeated spaces, tabs, blank lines, box-drawing characters, accented letters, and an emoji. Confirm spaces become `U+2003` and all other characters remain intact. Verify copied text in a Unicode-capable editor and confirm CRLF line endings. The source file must remain unchanged.
4. Resize the window and use both scrollbars. Long lines must not wrap. If available, move the window between monitors with different scaling settings; buttons and preview should stay usable.
5. Open a second file. Its output must replace the first. Click **Open text file...** again and cancel; the current preview and Copy button must remain unchanged.
6. Open an empty file and copy its result. The preview and copied text must be empty, with no crash. Reopen a nonempty file afterwards.
7. Check UTF-8 with a BOM, UTF-16 LE/BE with BOMs, and a legacy ANSI file matching the PC's active code page. Check a Unicode filename and a file with no trailing newline.
8. Open malformed BOM-marked Unicode, a null-containing file, an unreadable file, and a file larger than 32 MiB. Each must produce a readable error while keeping the previous result available.
9. Open a text file exceeding 32,768 characters. Confirm the complete result is visible and copied without truncation. **Copy result** must copy everything even when only part of the preview is selected.
10. Navigate using Tab, activate buttons using Space/Enter, and select/copy text from the preview. Confirm a clipboard error can be dismissed and copying retried (automated Windows tests also exercise a deliberately busy clipboard).

Publish only after recording these results. If Defender reports a named threat, retain the exact executable and report for Microsoft review before distributing it. A clean scan is not a guarantee against future detections.
