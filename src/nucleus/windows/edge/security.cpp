#include "security.h"

#include <windows.h>
#include <sddl.h>

#include <memory>

template <typename Callable>
struct on_scope_exit
{
  Callable _f;
  on_scope_exit(Callable f) : _f{std::move(f)} {}

  ~on_scope_exit()
  {
    _f();
  }
};

template <typename T>
struct typed_storage
{
  std::unique_ptr<char[]> _buffer;
  inline explicit typed_storage(const DWORD size) : _buffer{std::make_unique<char[]>(size)}
  {
  }
  inline operator T *()
  {
    return reinterpret_cast<T *>(_buffer.get());
  }
};

bool is_process_elevated()
{
  HANDLE token = nullptr;
  bool elevated = false;

  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
  {
    TOKEN_ELEVATION elevation;
    DWORD size;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
    {
      elevated = (elevation.TokenIsElevated != 0);
    }
  }

  if (token)
  {
    CloseHandle(token);
  }

  return elevated;
}

bool drop_elevated_privileges()
{
  HANDLE token = nullptr;
  LPCTSTR lpszPrivilege = SE_SECURITY_NAME;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_DEFAULT | WRITE_OWNER, &token))
  {
    return false;
  }

  PSID medium_sid = NULL;
  if (!::ConvertStringSidToSid(SDDL_ML_MEDIUM, &medium_sid))
  {
    return false;
  }

  TOKEN_MANDATORY_LABEL label = {0};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = medium_sid;
  DWORD size = (DWORD)sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(medium_sid);

  BOOL result = SetTokenInformation(token, TokenIntegrityLevel, &label, size);
  LocalFree(medium_sid);
  CloseHandle(token);

  return result;
}

bool initialize_com_security_policy_for_webview()
{
  const wchar_t *security_descriptor =
      L"O:BA" // Owner: Builtin (local) administrator
      L"G:BA" // Group: Builtin (local) administrator
      L"D:"
      L"(A;;0x7;;;PS)"                                                                                 // Access allowed on COM_RIGHTS_EXECUTE, _LOCAL, & _REMOTE for Personal self
      L"(A;;0x3;;;SY)"                                                                                 // Access allowed on COM_RIGHTS_EXECUTE, & _LOCAL for Local system
      L"(A;;0x7;;;BA)"                                                                                 // Access allowed on COM_RIGHTS_EXECUTE, _LOCAL, & _REMOTE for Builtin (local) administrator
      L"(A;;0x3;;;S-1-15-3-1310292540-1029022339-4008023048-2190398717-53961996-4257829345-603366646)" // Access allowed on COM_RIGHTS_EXECUTE, & _LOCAL for Win32WebViewHost package capability
      L"S:"
      L"(ML;;NX;;;LW)"; // Integrity label on No execute up for Low mandatory level
  PSECURITY_DESCRIPTOR self_relative_sd{};
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(security_descriptor, SDDL_REVISION_1, &self_relative_sd, nullptr))
  {
    return false;
  }

  on_scope_exit free_realtive_sd([&] {
    LocalFree(self_relative_sd);
  });

  DWORD absolute_sd_size = 0;
  DWORD dacl_size = 0;
  DWORD group_size = 0;
  DWORD owner_size = 0;
  DWORD sacl_size = 0;

  if (!MakeAbsoluteSD(self_relative_sd, nullptr, &absolute_sd_size, nullptr, &dacl_size, nullptr, &sacl_size, nullptr, &owner_size, nullptr, &group_size))
  {
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
      return false;
    }
  }

  typed_storage<SECURITY_DESCRIPTOR> absolute_sd{absolute_sd_size};
  typed_storage<ACL> dacl{dacl_size};
  typed_storage<ACL> sacl{sacl_size};
  typed_storage<SID> owner{owner_size};
  typed_storage<SID> group{group_size};

  if (!MakeAbsoluteSD(self_relative_sd,
                      absolute_sd,
                      &absolute_sd_size,
                      dacl,
                      &dacl_size,
                      sacl,
                      &sacl_size,
                      owner,
                      &owner_size,
                      group,
                      &group_size))
  {
    return false;
  }

  return !FAILED(CoInitializeSecurity(
      absolute_sd,
      -1,
      nullptr,
      nullptr,
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IDENTIFY,
      nullptr,
      EOAC_DYNAMIC_CLOAKING | EOAC_DISABLE_AAA,
      nullptr));
}
