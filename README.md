# WDP

Reads `ntkrnlmp.pdb` from the local symbol cache and prints kernel struct offsets and function RVAs. DIA SDK ships with Visual Studio, no other deps.

## Run

Populate the symbol cache once:

```
symchk /r C:\Windows\System32\ntoskrnl.exe /s SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols
```

Then:

```
WDP.exe                        # dumps _KPROCESS, _KTHREAD, common RVAs
WDP.exe kprocess               # just _KPROCESS
WDP.exe hooks                  # RVAs for a fixed list (edit in Dumpers.cpp)
WDP.exe exports                # every public symbol, sorted by RVA
WDP.exe --pdb <path\to.pdb>    # override PDB
```

Auto-picks the newest PDB under `C:\Symbols\ntkrnlmp.pdb\<GUID>\`.

## Build

Open `WinPdbDumper.slnx` in VS 2022, build Release x64. Binary is `WDP.exe`.
