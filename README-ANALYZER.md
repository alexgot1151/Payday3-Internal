Repo: https://github.com/Omega172/Payday3-Internal

Compiled DLL and PDB Provided along side the source for quick analysis.

Building:
Requires xmake, and an installation of VisualStudio with the C++ build tools for the compiler.

in the project root, run the following commands:

```cmd
xmake config -m debug   # or -m release
xmake build Payday3-Internal
```

Output: `Build/Debug/` or `Build/Release/`

The resulting DLL should already be stripped of symbols.

In the uc download all we want is just the `Payday3-Internal.dll`. The source will be uploaded as a separate uc download.