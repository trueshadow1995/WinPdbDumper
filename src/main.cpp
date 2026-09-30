#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <string>
#include <string_view>

#include "Dumper/Dumpers.h"

namespace PrintStuff {
void print_usage() {
  std::wprintf(
      L"WDP -- Windows Kernel PDB dumper (offsets / RVAs)\n"
      L"\n"
      L"Usage:\n"
      L"  WDP.exe [command] [--pdb <path>]\n"
      L"\n"
      L"Commands (no command == 'all'):\n"
      L"  all              Run every dump in order\n"
      L"  kprocess         _KPROCESS field offsets\n"
      L"  eprocess         _EPROCESS field offsets\n"
      L"  kthread          _KTHREAD + _KTRAP_FRAME field offsets\n"
      L"  ethread          _ETHREAD field offsets\n"
      L"  kpcr             _KPCR + _KPRCB field offsets\n"
      L"  mmaccessfault    nt!MmAccessFault RVA\n"
      L"  ntclose          nt!NtClose RVA\n"
      L"  hooks            RVAs for every ntoskrnl export the implant hooks\n"
      L"  exports          Every public symbol in the PDB (RVA-sorted, big)\n"
      L"\n"
      L"Options:\n"
      L"  --pdb <path>     Use a specific ntkrnlmp.pdb instead of the\n"
      L"                   newest one under C:\\Symbols\\ntkrnlmp.pdb\\.\n");
}

int run_command(const std::wstring_view command, const PdbSession& pdb) {
  std::wprintf(L"[pdb] %s\n\n", pdb.path().c_str());

  if (command == L"all") {
    dump_all(pdb);
  } else if (command == L"kprocess") {
    dump_kprocess(pdb);
  } else if (command == L"eprocess") {
    dump_eprocess(pdb);
  } else if (command == L"kthread") {
    dump_kthread_and_trapframe(pdb);
  } else if (command == L"ethread") {
    dump_ethread(pdb);
  } else if (command == L"kpcr") {
    dump_kpcr_kprcb(pdb);
  } else if (command == L"mmaccessfault") {
    dump_mmaccessfault(pdb);
  } else if (command == L"ntclose") {
    dump_ntclose(pdb);
  } else if (command == L"hooks") {
    dump_hooks(pdb);
  } else if (command == L"exports") {
    dump_all_exports(pdb);
  } else {
    std::fwprintf(stderr, L"unknown command: %.*s\n\n",
                  static_cast<int>(command.size()), command.data());
    print_usage();
    return 2;
  }
  return 0;
}
// True when nothing else shares our console == we were double-clicked.
bool launched_by_double_click() {
  DWORD pids[2];
  return GetConsoleProcessList(pids, 2) == 1;
}

// Interactive picker when double-clicked with no args. Returns chosen
// command or empty on ESC / invalid input (caller falls back to "all").
std::wstring prompt_menu() {
  static const struct { const wchar_t* key; const wchar_t* label; } kMenu[] = {
      { L"all",           L"Everything" },
      { L"kprocess",      L"_KPROCESS field offsets" },
      { L"eprocess",      L"_EPROCESS field offsets" },
      { L"kthread",       L"_KTHREAD + _KTRAP_FRAME" },
      { L"ethread",       L"_ETHREAD field offsets" },
      { L"kpcr",          L"_KPCR + _KPRCB" },
      { L"mmaccessfault", L"nt!MmAccessFault RVA" },
      { L"ntclose",       L"nt!NtClose RVA" },
      { L"hooks",         L"RVAs for the hooked ntoskrnl exports" },
      { L"exports",       L"Every public symbol (large)" },
  };
  const int N = (int)(sizeof(kMenu) / sizeof(kMenu[0]));

  std::wprintf(L"WDP -- pick a dump:\n\n");
  for (int i = 0; i < N; ++i)
    std::wprintf(L"  %2d.  %-15s  %s\n", i + 1, kMenu[i].key, kMenu[i].label);
  std::wprintf(L"\nchoice [1-%d, Enter=1]: ", N);

  wchar_t buf[16] = {};
  if (!std::fgetws(buf, 16, stdin)) return L"all";
  int pick = _wtoi(buf);
  if (pick <= 0) pick = 1;
  if (pick > N)  pick = 1;
  std::wprintf(L"\n");
  return kMenu[pick - 1].key;
}
}  // namespace PrintStuff

int wmain(const int argc, wchar_t* argv[]) {
  std::wstring command;
  std::wstring pdb_path;

  //   WDP.exe <command>
  //   WDP.exe <command> --pdb <path>
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view arg(argv[i]);

    if (arg == L"-h" || arg == L"--help") {
      PrintStuff::print_usage();
      return 0;
    } else if (arg == L"--pdb" && i + 1 < argc) {
      pdb_path = argv[++i];
    } else if (!arg.empty() && arg.front() != L'-') {
      command = arg;
    } else {
      std::fwprintf(stderr, L"unrecognised argument: %.*s\n\n",
                    static_cast<int>(arg.size()), arg.data());
      PrintStuff::print_usage();
      return 2;
    }
  }

  // No command supplied: menu if double-clicked, else default "all".
  if (command.empty()) {
    command = PrintStuff::launched_by_double_click()
                  ? PrintStuff::prompt_menu()
                  : L"all";
  }

  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
    std::fwprintf(stderr, L"CoInitializeEx failed\n");
    return 1;
  }

  int exit_code = 0;

  try {
    PdbSession pdb(pdb_path);
    exit_code = PrintStuff::run_command(command, pdb);
  } catch (const std::exception& ex) {
    std::fwprintf(stderr, L"error: %hs\n", ex.what());
    exit_code = 1;
  }

  CoUninitialize();

  // Only pause when double-clicked (nobody else owns the console).
  // Piped / CLI runs exit immediately so scripts aren't blocked.
  if (PrintStuff::launched_by_double_click()) {
    std::wprintf(L"\nPress Enter to exit...\n");
    std::getwchar();
  }
  return exit_code;
}
