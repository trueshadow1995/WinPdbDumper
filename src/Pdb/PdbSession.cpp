#include "PdbSession.h"

#include <diacreate.h>

#include <stdexcept>

namespace fs = std::filesystem;

namespace dia_loader {
constexpr const wchar_t* kDiaDllNames[] = {
    L"msdia140.dll", L"msdia150.dll", L"msdia160.dll",
    L"msdia170.dll", L"msdia180.dll",
};

constexpr const wchar_t* kDiaSubdirs[] = {
    L"\\DIA SDK\\bin\\amd64\\",
    L"\\Common7\\IDE\\",
};

std::wstring resolve_msdia_full_path() {
  wchar_t pf_buf[MAX_PATH] = {};
  DWORD pf_len = GetEnvironmentVariableW(L"ProgramFiles", pf_buf, MAX_PATH);
  if (pf_len == 0 || pf_len >= MAX_PATH) return {};

  const fs::path vs_root = fs::path(pf_buf) / L"Microsoft Visual Studio";
  if (!fs::exists(vs_root)) return {};

  for (const auto& year_dir : fs::directory_iterator(vs_root)) {
    if (!year_dir.is_directory()) continue;

    for (const auto& edition_dir : fs::directory_iterator(year_dir)) {
      if (!edition_dir.is_directory()) continue;

      for (const wchar_t* subdir : kDiaSubdirs) {
        for (const wchar_t* dll : kDiaDllNames) {
          const fs::path candidate =
              fs::path(edition_dir.path().wstring() + subdir + dll);
          if (fs::exists(candidate)) return candidate.wstring();
        }
      }
    }
  }
  return {};
}

CComPtr<IDiaDataSource> create_data_source() {
  const std::wstring full_path = resolve_msdia_full_path();

  if (!full_path.empty()) {
    CComPtr<IDiaDataSource> src;
    const HRESULT hr =
        NoRegCoCreate(full_path.c_str(), __uuidof(DiaSource),
                      __uuidof(IDiaDataSource), reinterpret_cast<void**>(&src));
    if (SUCCEEDED(hr) && src) return src;
  }

  CComPtr<IDiaDataSource> src;
  const HRESULT hr = src.CoCreateInstance(__uuidof(DiaSource));
  if (SUCCEEDED(hr) && src) return src;

  return {};
}
}  // namespace dia_loader

std::wstring PdbSession::find_newest_pdb() {
  const fs::path root = L"C:\\Symbols\\ntkrnlmp.pdb";
  if (!fs::exists(root)) {
    throw std::runtime_error(
        "C:\\Symbols\\ntkrnlmp.pdb does not exist "
        "-- populate it with symchk or WinDbg first.");
  }

  fs::path newest;
  fs::file_time_type newest_time{};

  for (const auto& guid_dir : fs::directory_iterator(root)) {
    if (!guid_dir.is_directory()) continue;

    const fs::path candidate = guid_dir.path() / L"ntkrnlmp.pdb";
    if (!fs::exists(candidate)) continue;

    const auto mtime = fs::last_write_time(candidate);
    if (newest.empty() || mtime > newest_time) {
      newest = candidate;
      newest_time = mtime;
    }
  }

  if (newest.empty()) {
    throw std::runtime_error("no ntkrnlmp.pdb found under any GUID directory");
  }

  return newest.wstring();
}

PdbSession::PdbSession(const std::wstring& pdb_path) {
  m_pdb_path = pdb_path.empty() ? find_newest_pdb() : pdb_path;

  m_source = dia_loader::create_data_source();
  if (!m_source) {
    throw std::runtime_error(
        "DIA SDK not available (msdia*.dll missing "
        "from the safe DLL search path)");
  }

  HRESULT hr = m_source->loadDataFromPdb(m_pdb_path.c_str());
  if (FAILED(hr)) {
    throw std::runtime_error("loadDataFromPdb failed for the selected PDB");
  }

  hr = m_source->openSession(&m_session_holder);
  if (FAILED(hr)) {
    throw std::runtime_error("openSession failed");
  }

  hr = m_session_holder->get_globalScope(&m_globals_holder);
  if (FAILED(hr)) {
    throw std::runtime_error("get_globalScope failed");
  }

  m_session = m_session_holder.p;
  m_globals = m_globals_holder.p;
}
