#include "Dumpers.h"

#include <atlbase.h>
#include <dia2.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#include "../Pdb/PdbSession.h"

namespace Pdb_Util {
struct BstrGuard {
  BSTR value = nullptr;
  ~BstrGuard() {
    if (value) SysFreeString(value);
  }
  BstrGuard() = default;
  BstrGuard(const BstrGuard&) = delete;
  BstrGuard& operator=(const BstrGuard&) = delete;
};

std::string narrow(const BSTR bstr) {
  if (!bstr) return {};
  const int len =
      WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) return {};

  std::string out(static_cast<size_t>(len - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, bstr, -1, out.data(), len, nullptr, nullptr);
  return out;
}

CComPtr<IDiaSymbol> find_udt(const PdbSession& pdb, const wchar_t* name) {
  CComPtr<IDiaEnumSymbols> enum_syms;
  if (FAILED(
          pdb.globals()->findChildren(SymTagUDT, name, nsNone, &enum_syms))) {
    return nullptr;
  }

  CComPtr<IDiaSymbol> udt;
  ULONG celt = 0;
  while (SUCCEEDED(enum_syms->Next(1, &udt, &celt)) && celt == 1) {
    DWORD sym_tag = 0;
    udt->get_symTag(&sym_tag);
    if (sym_tag == SymTagUDT) {
      return udt;
    }
    udt.Release();
  }
  return nullptr;
}

bool name_matches_any_filter(const std::wstring& name,
                             std::initializer_list<std::wstring_view> filters) {
  if (filters.size() == 0) return true;

  for (const auto& needle : filters) {
    if (name.find(needle) != std::wstring::npos) return true;
  }
  return false;
}
}  // namespace Pdb_Util

void dump_struct_fields(const PdbSession& pdb, const wchar_t* struct_name,
                        std::initializer_list<std::wstring_view> name_filters) {
  const CComPtr<IDiaSymbol> udt = Pdb_Util::find_udt(pdb, struct_name);
  if (!udt) {
    std::wprintf(L"[%s] not found in PDB\n", struct_name);
    return;
  }

  ULONGLONG size_bytes = 0;
  udt->get_length(&size_bytes);
  std::wprintf(L"struct %s  size=0x%llX\n", struct_name, size_bytes);

  CComPtr<IDiaEnumSymbols> members;
  if (FAILED(udt->findChildren(SymTagData, nullptr, nsNone, &members))) {
    std::wprintf(L"  <no member enumeration>\n");
    return;
  }

  CComPtr<IDiaSymbol> field;
  ULONG celt = 0;
  while (SUCCEEDED(members->Next(1, &field, &celt)) && celt == 1) {
    DWORD data_kind = 0;
    field->get_dataKind(&data_kind);

    if (data_kind == DataIsMember) {
      Pdb_Util::BstrGuard name_guard;
      field->get_name(&name_guard.value);

      LONG offset = 0;
      field->get_offset(&offset);

      const std::wstring name = name_guard.value
                                    ? std::wstring(name_guard.value)
                                    : std::wstring(L"<unnamed>");

      if (Pdb_Util::name_matches_any_filter(name, name_filters)) {
        std::wprintf(L"  +0x%04lX  %s\n", static_cast<unsigned long>(offset),
                     name.c_str());
      }
    }

    field.Release();
  }
}

void dump_symbol_rva(const PdbSession& pdb, const wchar_t* symbol_name) {
  CComPtr<IDiaEnumSymbols> enum_syms;

  const DWORD tags[] = {SymTagPublicSymbol, SymTagFunction, SymTagData};

  for (const DWORD tag : tags) {
    enum_syms.Release();
    if (FAILED(pdb.globals()->findChildren(static_cast<enum SymTagEnum>(tag),
                                           symbol_name, nsNone, &enum_syms))) {
      continue;
    }

    CComPtr<IDiaSymbol> sym;
    ULONG celt = 0;
    if (SUCCEEDED(enum_syms->Next(1, &sym, &celt)) && celt == 1) {
      DWORD rva = 0;
      sym->get_relativeVirtualAddress(&rva);
      std::wprintf(L"  %s  RVA=0x%08lX\n", symbol_name,
                   static_cast<unsigned long>(rva));
      return;
    }
  }

  std::wprintf(L"  %s  not found\n", symbol_name);
}

void dump_kprocess(const PdbSession& pdb) {
  std::wprintf(L"---- _KPROCESS ----\n");
  dump_struct_all_fields(pdb, L"_KPROCESS");
  std::wprintf(L"\n");
}

void dump_kthread_and_trapframe(const PdbSession& pdb) {
  std::wprintf(L"---- _KTHREAD ----\n");
  dump_struct_all_fields(pdb, L"_KTHREAD");

  std::wprintf(L"\n---- _KTRAP_FRAME ----\n");
  dump_struct_all_fields(pdb, L"_KTRAP_FRAME");
  std::wprintf(L"\n");
}

void dump_eprocess(const PdbSession& pdb) {
  std::wprintf(L"---- _EPROCESS ----\n");
  dump_struct_all_fields(pdb, L"_EPROCESS");
  std::wprintf(L"\n");
}

void dump_ethread(const PdbSession& pdb) {
  std::wprintf(L"---- _ETHREAD ----\n");
  dump_struct_all_fields(pdb, L"_ETHREAD");
  std::wprintf(L"\n");
}

void dump_kpcr_kprcb(const PdbSession& pdb) {
  std::wprintf(L"---- _KPCR ----\n");
  dump_struct_all_fields(pdb, L"_KPCR");

  std::wprintf(L"\n---- _KPRCB ----\n");
  dump_struct_all_fields(pdb, L"_KPRCB");
  std::wprintf(L"\n");
}

void dump_mmaccessfault(const PdbSession& pdb) {
  std::wprintf(L"---- nt!MmAccessFault ----\n");
  dump_symbol_rva(pdb, L"MmAccessFault");
  dump_symbol_rva(pdb, L"MiAccessFault");
  std::wprintf(L"\n");
}

void dump_ntclose(const PdbSession& pdb) {
  std::wprintf(L"---- nt!NtClose ----\n");
  dump_symbol_rva(pdb, L"NtClose");
  dump_symbol_rva(pdb, L"ZwClose");
  std::wprintf(L"\n");
}

void dump_all_exports(const PdbSession& pdb) {
  std::wprintf(L"---- all public symbols ----\n");

  CComPtr<IDiaEnumSymbols> enum_syms;
  if (FAILED(pdb.globals()->findChildren(SymTagPublicSymbol, nullptr, nsNone,
                                         &enum_syms))) {
    std::wprintf(L"  <enumeration failed>\n");
    return;
  }

  struct Entry {
    DWORD rva;
    std::wstring name;
  };
  std::vector<Entry> entries;
  entries.reserve(16384);

  CComPtr<IDiaSymbol> sym;
  ULONG celt = 0;
  while (SUCCEEDED(enum_syms->Next(1, &sym, &celt)) && celt == 1) {
    DWORD rva = 0;
    sym->get_relativeVirtualAddress(&rva);

    Pdb_Util::BstrGuard name_guard;
    sym->get_name(&name_guard.value);

    if (rva != 0 && name_guard.value) {
      entries.push_back({rva, std::wstring(name_guard.value)});
    }
    sym.Release();
  }

  std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.rva < b.rva; });

  for (const Entry& e : entries) {
    std::wprintf(L"  0x%08lX  %s\n", static_cast<unsigned long>(e.rva),
                 e.name.c_str());
  }
  std::wprintf(L"\n[%zu symbols]\n", entries.size());
}

void dump_hooks(const PdbSession& pdb) {
  // Every ntoskrnl export / private routine, used for a project of mine.
  // Remove or add as needed
  const wchar_t* symbols[] = {
      L"NtClose",
      L"MmAccessFault",
      L"KeGetCurrentThread",
      L"KeGetCurrentIrql",
      L"MmGetPhysicalAddress",
      L"ExAllocatePool2",
      L"ExFreePool",
      L"PsGetCurrentProcess",
      L"PsGetProcessImageFileName",
      L"PsLookupProcessByProcessId",
      L"KeStackAttachProcess",
      L"KeUnstackDetachProcess",
      L"ObfDereferenceObject",
  };

  std::wprintf(L"---- resolved kernel exports / hooks ----\n");
  for (const wchar_t* sym : symbols) {
    dump_symbol_rva(pdb, sym);
  }
  std::wprintf(L"\n");
}

void dump_all(const PdbSession& pdb) {
  dump_kprocess(pdb);
  dump_eprocess(pdb);
  dump_kthread_and_trapframe(pdb);
  dump_ethread(pdb);
  dump_kpcr_kprcb(pdb);
  dump_mmaccessfault(pdb);
  dump_ntclose(pdb);
  dump_hooks(pdb);
}
