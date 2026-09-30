#pragma once
#include <atlbase.h>
#include <dia2.h>
#include <windows.h>

#include <filesystem>
#include <string>

class PdbSession {
 public:
  explicit PdbSession(const std::wstring& pdb_path = L"");

  IDiaSession* session() const { return m_session; }
  IDiaSymbol* globals() const { return m_globals; }
  const std::wstring& path() const { return m_pdb_path; }

 private:
  static std::wstring find_newest_pdb();

  CComPtr<IDiaDataSource> m_source;
  CComPtr<IDiaSession> m_session_holder;
  CComPtr<IDiaSymbol> m_globals_holder;

  IDiaSession* m_session = nullptr;
  IDiaSymbol* m_globals = nullptr;

  std::wstring m_pdb_path;
};
