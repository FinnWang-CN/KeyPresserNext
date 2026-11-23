# KeyPresserNext Community

**English** · [简体中文](README.zh-CN.md)

KeyPresserNext Community is a Windows desktop app for recording keyboard and mouse input and editing it as a sequence of actions. A row can send a key, click a point, or look for an image in the target window before acting.

![KeyPresserNext Community action editor](docs/images/ui-main.png)

This repository contains the Community source. The **Official edition 1.2.5** installer in Releases is the separately distributed product. [Official website](https://keypressernext.onepeak.cn/en.html) · [Original download page](https://keypresser.onepeak.cn/keypresser-next-en.html)

## What you can do with Community

- Select a target window, keep it on top, and choose capture, keyboard and mouse input modes.
- Press **F8** to record. Edit the resulting rows, import actions in bulk, or undo and redo changes.
- Run enabled rows in sequence or independently. Set the repeat count, intervals and playback speed.
- Save a workflow as a `.kpn` file. Copy its companion `.assets` folder with the file to keep its screenshots.

## Community 1.0.0 and Official 1.2.5

| Feature | Community source in this repository | Official Windows edition |
| --- | --- | --- |
| Record, edit and replay actions; classic template matching | Yes | Yes |
| Application source available to modify and build | Yes | Distributed as a binary |
| Interface language | Simplified Chinese | English and Simplified Chinese; select in Settings and reopen the app |
| Windows installer | Build the Chinese installer with the repository's release script | Bilingual installer on GitHub and GitCode |
| Multiscale and feature-based image matching | — | Yes |
| Locate text before a click or keystroke | — | Yes, with the separate OCR component |
| Drag to reorder action rows | — | Yes |
| Runtime DLLs for input into 32-bit targets | — | Included |
| 16-bit color compatibility setting | — | Yes |
| DeepSeek Harness plugin and MCP server | — | Both included |
| Built-in app and OCR updates | — | Yes |
| Online membership and licensing | — | Yes |

## Agent integration in the official edition

**DeepSeek Harness** support was introduced in 1.2.0. Choose **Help → Install DSH plugin and restart** to register the tool in your local web profile. The agent can select a window by title or handle, edit rows, change timing, start or stop a run or recording, test capture and input, and open or save configurations. Changes appear in the desktop editor. The plugin also registers the user guide as a skill. [DeepSeek Harness](https://github.com/deepseek-ai/deepseek-harness)

**MCP** support was introduced in 1.2.1. Clients such as Cursor, Claude Code, Codex, Trae and CodeBuddy can use the same local control channel. Install Node.js and run the client under the same Windows account as KeyPresserNext. Choose **Help → Copy MCP configuration**, merge it into the client's configuration, then refresh its tools. The tool is named `keypressernext`.

## Build Community on Windows

1. Install Qt 5.15 with a 64-bit MSVC kit.
2. Use the bundled QWindowKit installation in `third_party/qwindowkit-install`. If it is missing, run `scripts\build_qwindowkit.bat`.
3. Open `KeyPresserNext.pro` in Qt Creator, select the 64-bit kit, run qmake and build. The executable is `KeyPresserNextCommunity.exe`.
4. Keep `NextCore.dll`, `NextBase.dll` and `tools.dll` beside the executable. The build copies them from `engine/runtime/bin`.

The release executable requests administrator privileges so its hotkeys and recording can work with elevated target windows. To build a Community installer, run `scripts\release.bat` with Inno Setup installed.

## License

The Community application source uses the MIT license. Third-party libraries and runtime components have their own notices. [LICENSE](LICENSE) · [NOTICE](NOTICE)
