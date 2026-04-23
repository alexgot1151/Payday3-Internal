# Note
> UnknownCheats Thread: https://www.unknowncheats.me/forum/payday-3-a/736601-internal-cheeto.html
>
> Join our [discord](https://discord.gg/zc8E7dYYRe)

# Payday 3 Internal
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/Omega172/Payday3-Internal/total)
[![Discord](https://img.shields.io/discord/1209142443627257856)](https://discord.gg/zc8E7dYYRe)

An cheat for Payday 3 targeting the steam build.

Download the latest DLL from [here](https://github.com/Omega172/Payday3-Internal/releases/latest)

Download the injector from [here](https://github.com/Omega172/Payday3-Internal/releases/tag/Injector)

## Building

Requires xmake, and an installation of VisualStudio with the C++ build tools for the compiler.

```cmd
xmake config -m debug   # or -m release
xmake build Payday3-Internal
```

Output: `Build/Debug/` or `Build/Release/`

**Note:** If IntelliSense breaks in VS Code, run `update_compile_commands.bat` to rebuild compile commands and potentially fix IntelliSense.

### Dependencies (via xmake repo)
- minhook
- imgui (with win32-binding, dx12-binding)

## Usage

### Windows
- **Build**: Follow the build instructions above to compile the project.
- **Remove Streamline DLSS**: I have yet to find a fix so stop streamline from crashing the game upon init of the dx12 hook so delete all the files in `C:\Path\To\PAYDAY3\Engine\Plugins\Runtime\Nvidia\Streamline\Binaries\ThirdParty\Win64`
- **Injection**: Use any DLL injector to inject `Payday3-Internal.dll` into the `PAYDAY3Client-Win64-Shipping.exe` process.
- **In-Game**: Press `INSERT` to open the menu and `END` to unload the cheat.

### Proton
- **Supports Proton:** As of v1.2.13b & #6, the cheat should work on both Windows and Linux (proton) versions of the game. Thanks to [alexgot1151](https://github.com/alexgot1151)
- TODO: Write proton instructions
