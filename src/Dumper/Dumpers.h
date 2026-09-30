#pragma once

#include <initializer_list>
#include <string_view>

#include "../Pdb/PdbSession.h"

void dump_struct_fields(const PdbSession& pdb, const wchar_t* struct_name,
                        std::initializer_list<std::wstring_view> name_filters);

void dump_symbol_rva(const PdbSession& pdb, const wchar_t* symbol_name);

inline void dump_struct_all_fields(const PdbSession& pdb,
                                   const wchar_t* struct_name) {
  dump_struct_fields(pdb, struct_name, {});
}

void dump_kprocess(const PdbSession& pdb);
void dump_eprocess(const PdbSession& pdb);
void dump_kthread_and_trapframe(const PdbSession& pdb);
void dump_ethread(const PdbSession& pdb);
void dump_kpcr_kprcb(const PdbSession& pdb);
void dump_mmaccessfault(const PdbSession& pdb);
void dump_ntclose(const PdbSession& pdb);
void dump_hooks(const PdbSession& pdb);
void dump_all_exports(const PdbSession& pdb);
void dump_all(const PdbSession& pdb);
