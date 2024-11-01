
#include "LiveDump.hpp"

#include <string>
#include <vector>

//
// Globals
//
static NtSystemDebugControl g_NtSystemDebugControl = NULL;

///=========================================================================
/// PrintUsage()
///
/// <summary>
/// Print program usage
/// </summary>
/// <returns></returns>
/// <remarks>
/// </remarks>
///=========================================================================
VOID PrintUsage(VOID)
{
  printf("\n\n");
  printf("LiveDump.exe [type] [options] <FileName>\n");
  printf("Type:\n");
  printf("\ttriage : create a kernel triage dump (parameter 29)\n");
  printf("\tkernel : create a kernel live dump (parameter 37)\n");
  printf("Options (kernel triage dump only):\n");
  printf("\t-t : Number of threads to include in the dump. Should be between 1 and 16. Default is 4.\n");
  printf("\t-p : PID to dump\n");
  printf("Options (kernel live dump only):\n");
  printf("\t-c : compress memory pages in dump\n");
  printf("\t-d : Use dump stack\n");
  printf("\t-h : add hypervisor pages\n");
  printf("\t-u : also dump user space memory (possible starting with Windows 11 22H2)\n");
  printf("FileName is the full path to the dump file to create.");
  printf("\n");
}


///=========================================================================

// https://stackoverflow.com/a/17387176/3740047
std::string GetLastErrorAsString()
{
  // Get the error message ID, if any.
  DWORD errorMessageID = ::GetLastError();
  if (errorMessageID == 0) {
    return std::string(); // No error message has been recorded
  }

  LPSTR messageBuffer = nullptr;

  // Ask Win32 to give us the string version of that message ID.
  // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know
  // how long the message string will be).
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      errorMessageID,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer,
      0,
      NULL);

  // Copy the error message into a std::string.
  std::string message(messageBuffer, size);

  // Free the Win32's string's buffer.
  LocalFree(messageBuffer);

  return message;
}

///=========================================================================
/// main()
///
/// <summary>
/// Main console program
/// </summary>
/// <returns>0 on success, 1 on failure</returns>
/// <remarks>
/// </remarks>
///=========================================================================
INT wmain(__in INT Argc, __in PWCHAR Argv[])
{
  HANDLE handle;
  HMODULE module;
  DWORD result;
  INT i;
  SYSDBG_LIVEDUMP_CONTROL_FLAGS flags;
  SYSDBG_LIVEDUMP_CONTROL_ADDPAGES pages;
  PWCHAR outfile;
  PWCHAR type;
  NTSTATUS status;
  ULONG pid;
  ULONG fileFlags;
  ULONG numThreads = 4;

  handle = INVALID_HANDLE_VALUE;
  result = NO_ERROR;
  flags.AsUlong = 0;
  pages.AsUlong = 0;
  outfile = NULL;
  type = NULL;
  pid = 0;

  //
  // Parse and validate arguments
  //
  if (Argc < 3) {
    printf("Invalid number of arguments.");
    PrintUsage();
    return 1;
  }

  for (i = 1; i < Argc; ++i) {
    if (i == 1) {
      type = Argv[i];
      continue;
    }

    if (_wcsicmp(Argv[i], L"-c") == 0) {
      flags.CompressMemoryPagesData = 1;
    }
    else if (_wcsicmp(Argv[i], L"-d") == 0) {
      flags.UseDumpStorageStack = 1;
    }
    else if (_wcsicmp(Argv[i], L"-h") == 0) {
      pages.HypervisorPages = 1;
    }
    else if (_wcsicmp(Argv[i], L"-u") == 0) {
      flags.IncludeUserSpaceMemoryPages = 1;
    }
    else if (_wcsicmp(Argv[i], L"-t") == 0) {
      if ((i + 1) >= Argc) {
        printf("You must specify the number of threads.\n");
        PrintUsage();
        return 1;
      }

      numThreads = wcstoul(Argv[++i], '\0', 10);

      if (numThreads == ULONG_MAX) {
        printf("Invalid number of threads specified.\n");
        PrintUsage();
        return 1;
      }
    }
    else if (_wcsicmp(Argv[i], L"-p") == 0) {
      if ((i + 1) >= Argc) {
        printf("You must specify a PID.\n");
        PrintUsage();
        return 1;
      }

      pid = wcstoul(Argv[++i], '\0', 10);

      if (pid == ULONG_MAX) {
        printf("Invalid PID.\n");
        PrintUsage();
        return 1;
      }
    }
    else {
      outfile = Argv[i];
    }
  }

  if (outfile == NULL) {
    printf("You must specify a file name.\n");
    PrintUsage();
    return 1;
  }

  if (_wcsicmp(type, L"triage") == 0) {
    //
    // WriteFile can't cope with buffers that aren't sector-aligned when we specify
    // FILE_FLAG_NO_BUFFERING (which is a requirement for kernel dump creation).
    // Since the returned triage buffer data can be unaligned in this manner, it's
    // easiest to just prevent WriteFile failing by not specifying that flag during
    // the call to CreateFile.
    //
    fileFlags = FILE_ATTRIBUTE_NORMAL;

    if (pid == 0) {
      printf("A non-zero PID is required for triage dumps.\n");
      PrintUsage();
      return 1;
    }
  }
  else if (_wcsicmp(type, L"kernel") == 0) {
    //
    // We have to use synchronous/no-buffering I/O for kernel dump creation.
    //
    fileFlags = FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;
  }
  else {
    printf("Valid dump types are 'triage' and 'kernel'.\n");
    PrintUsage();
    return 1;
  }

  //
  // Get function addresses
  //
  module = LoadLibrary(L"ntdll.dll");

  if (module == NULL) {
    printf("Failed to load ntdll.dll\n");
    return 1;
  }

  g_NtSystemDebugControl = (NtSystemDebugControl)GetProcAddress(module, "NtSystemDebugControl");

  FreeLibrary(module);

  if (g_NtSystemDebugControl == NULL) {
    printf("Failed to resolve NtSystemDebugControl.\n");
    return 1;
  }

  //
  // Get SeDebugPrivilege
  //
  if (!EnablePrivilege(SE_DEBUG_NAME, TRUE)) {
    result = GetLastError();
    printf("Failed to enable SeDebugPrivilege:  %lu\n", result);
    goto Exit;
  }

  //
  // Create the target file (must specify synchronous I/O)
  //
  handle = CreateFileW(outfile, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS, fileFlags, NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    result = GetLastError();
    std::string const resultStr = GetLastErrorAsString();
    printf("CreateFileW failed: %s (%d)\n", resultStr.c_str(), result);
    goto Exit;
  }

  //
  // Try to create the requested dump
  //
  if (_wcsicmp(type, L"triage") == 0) {
    status = CreateTriageDump(handle, pid, numThreads);
  }
  else if (_wcsicmp(type, L"kernel") == 0) {
    status = CreateKernelDump(handle, flags, pages);
  }

  if (NT_SUCCESS(status)) {
    printf("Dump file '%ws' written successfully!\n", outfile);
    result = NO_ERROR;
  }
  else {
    printf("Failed to create dump file.\n");
    result = 1;
  }

Exit:

  //
  // Remove privileges regardless of earlier success.
  //
  EnablePrivilege(SE_DEBUG_NAME, FALSE);

  if (handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle);

    if (!NT_SUCCESS(status)) {
      DeleteFile(outfile);
    }
  }

  return (result == NO_ERROR) ? 0 : 1;
}

///=========================================================================
/// EnablePrivilege()
///
/// <summary>
/// Enables or disables a privilege in an access token
/// </summary>
/// <parameter>PrivilegeName - name of privilege</parameter>
/// <parameter>Acquire - TRUE to add, FALSE to remove</parameter>
/// <returns>TRUE on success, FALSE on failure</returns>
/// <remarks>
/// </remarks>
///=========================================================================
BOOL EnablePrivilege(__in PCWSTR PrivilegeName, __in BOOLEAN Acquire)
{
  HANDLE tokenHandle;
  BOOL ret;
  ULONG tokenPrivilegesSize = FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges[1]);
  PTOKEN_PRIVILEGES tokenPrivileges = static_cast<PTOKEN_PRIVILEGES>(calloc(1, tokenPrivilegesSize));

  if (tokenPrivileges == NULL) {
    printf("Failed to allocate token privileges structure\n");
    return FALSE;
  }

  tokenHandle = NULL;
  tokenPrivileges->PrivilegeCount = 1;
  ret = LookupPrivilegeValue(NULL, PrivilegeName, &tokenPrivileges->Privileges[0].Luid);
  if (ret == FALSE) {
    printf("Failed to lookup privilege value by name:  %lu\n", GetLastError());
    goto Exit;
  }

  tokenPrivileges->Privileges[0].Attributes = Acquire ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;

  ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle);
  if (ret == FALSE) {
    printf("Failed to open current process token:  %lu\n", GetLastError());
    goto Exit;
  }

  ret = AdjustTokenPrivileges(tokenHandle, FALSE, tokenPrivileges, tokenPrivilegesSize, NULL, NULL);
  if (ret == FALSE) {
    printf("Failed to adjust current process token privileges:  %lu\n", GetLastError());
    goto Exit;
  }

Exit:

  if (tokenHandle != NULL) {
    CloseHandle(tokenHandle);
  }

  free(tokenPrivileges);

  return ret;
}

///=========================================================================
/// CreateTriageDump()
///
/// <summary>
/// Creates a triage dump using NtDebugSystemControl parameter 29 and the
/// first 16 threads of the supplied process.
/// </summary>
/// <parameter>FileHandle - Handle to dump file to write</parameter>
/// <parameter>Pid - ID of the process to dump</parameter>
/// <returns>NTSTATUS code</returns>
/// <remarks>
/// </remarks>
///=========================================================================
NTSTATUS CreateTriageDump(__in HANDLE FileHandle, __in ULONG Pid, __in ULONG numThreads)
{
  NTSTATUS status;
  SYSDBG_TRIAGE_DUMP dump;
  PUCHAR dumpData;
  ULONG returnLength;
  ULONG bytesWritten;
  HANDLE enumHandle;
  THREADENTRY32 thread;
  HANDLE processHandle;
  std::vector<HANDLE> threadHandles;

  printf("Attempting to create a triage dump...\n");

  enumHandle = NULL;
  dumpData = NULL;
  status = -1;

  //
  // Open the process
  //
  processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, Pid);

  if (processHandle == INVALID_HANDLE_VALUE) {
    printf("Failed to open PID %lu: %lu", Pid, GetLastError());
    goto Exit;
  }

  //
  // Enumerate the first 16 threads
  //
  enumHandle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, Pid);

  if (enumHandle == INVALID_HANDLE_VALUE) {
    printf("Failed to get thread list:  %lu", GetLastError());
    goto Exit;
  }

  thread.dwSize = sizeof(thread);

  if (!Thread32First(enumHandle, &thread)) {
    printf("Failed to get first thread:  %lu", GetLastError());
    goto Exit;
  }

  unsigned int totalNumThreads = 0;
  do {
    if (thread.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(thread.th32OwnerProcessID)) {
      if (thread.th32OwnerProcessID != Pid) {
        continue;
      }

      HANDLE const threadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, thread.th32ThreadID);
      if (threadHandle == NULL) {
        continue;
      }

      if (threadHandle == INVALID_HANDLE_VALUE) {
        printf("Failed to open thread %lu, skipping.\n", thread.th32ThreadID);
        continue;
      }

      PWSTR threadNameData;
      HRESULT hr = GetThreadDescription(threadHandle, &threadNameData);
      if (FAILED(hr)) {
        printf("Failed to get name of thread %lu, skipping.\n", thread.th32ThreadID);
        continue;
      }

      std::wstring threadName(threadNameData);
      LocalFree(threadNameData);
      DWORD const crosscheckTID = GetThreadId(threadHandle);

      if (threadHandles.size() < numThreads) {
        wprintf(L"Dumping thread %6lu (%6lu), name \"%s\".\n", thread.th32ThreadID, crosscheckTID, threadName.c_str());
        threadHandles.push_back(threadHandle);
      }
      else {
        wprintf(
            L"Not using thread %6lu (%6lu), name \"%s\", numThreads=%lu exceeded.\n",
            thread.th32ThreadID,
            crosscheckTID,
            threadName.c_str(),
            numThreads);
      }

      ++totalNumThreads;
    }
  }
  while ((Thread32Next(enumHandle, &thread)));


  if (threadHandles.empty()) {
    printf("No suitable threads found in PID %lu\n", Pid);
    goto Exit;
  }

  printf(
      "Triage dump is for PID %lu with %llu threads (out of %lu threads).\n",
      Pid,
      threadHandles.size(),
      totalNumThreads);

  //
  // Allocate buffer for triage dump data
  //
  dumpData = (PUCHAR)(calloc(1, TRIAGE_SIZE));

  if (dumpData == NULL) {
    printf("Failed to allocate %lu bytes\n", TRIAGE_SIZE);
    goto Exit;
  }

  memset(&dump, 0, sizeof(dump));
  memset(dumpData, 0, TRIAGE_SIZE);

  dump.ThreadHandles = (ULONG)threadHandles.size();
  dump.Handles = threadHandles.data();

  assert(g_NtSystemDebugControl != NULL);
  status
      = g_NtSystemDebugControl(SysDbgGetTriageDump, (PVOID)(&dump), sizeof(dump), dumpData, TRIAGE_SIZE, &returnLength);

  if (!NT_SUCCESS(status)) {
    printf("NtSystemDebugControl failed:  %08x\n", status);
    goto Exit;
  }

  printf("NtSystemDebugControl succeeded. ReturnLength = %lu. (TRIAGE_SIZE=%lu)\n", returnLength, TRIAGE_SIZE);

  if (returnLength == 0) {
    printf("Triage data buffer is empty.  Try a different process.\n");
    status = -1;
    goto Exit;
  }

  //
  // Write to target dump file
  //
  if (!WriteFile(FileHandle, dumpData, returnLength, &bytesWritten, NULL)) {
    printf("WriteFile failed: %lu\n", GetLastError());
    status = -1;
    goto Exit;
  }

  assert(bytesWritten == returnLength);

Exit:

  if (enumHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(enumHandle);
  }

  for (HANDLE handle : threadHandles) {
    CloseHandle(handle);
  }

  if (processHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(processHandle);
  }

  if (dumpData != NULL) {
    free(dumpData);
  }

  return status;
}

///=========================================================================
/// CreateKernelDump()
///
/// <summary>
/// Creates a kernel dump using NtDebugSystemControl parameter 37
/// </summary>
/// <parameter>FileHandle - Handle to dump file to write</parameter>
/// <parameter>Flags - Kernel dump flags</parameter>
/// <parameter>Pages - Flags for memory pages to write</parameter>
/// <returns>NTSTATUS code</returns>
/// <remarks>
/// </remarks>
///=========================================================================
NTSTATUS CreateKernelDump(
    __in HANDLE FileHandle,
    __in SYSDBG_LIVEDUMP_CONTROL_FLAGS Flags,
    __in SYSDBG_LIVEDUMP_CONTROL_ADDPAGES Pages)
{
  NTSTATUS status;
  SYSDBG_LIVEDUMP_CONTROL liveDumpControl;
  ULONG returnLength;

  printf("Attempting to create a kernel dump with flags %08x and pages %08x...\n", Flags.AsUlong, Pages.AsUlong);
  printf("Please be patient, this could take a minute or two...\n");

  memset(&liveDumpControl, 0, sizeof(liveDumpControl));

  //
  // The only thing the kernel looks at in the struct we pass is the handle,
  // the flags and the pages to dump.
  //
  liveDumpControl.DumpFileHandle = (PVOID)(FileHandle);
  liveDumpControl.AddPagesControl = Pages;
  liveDumpControl.Flags = Flags;

  status = g_NtSystemDebugControl(
      SysDbgGetLiveKernelDump,
      (PVOID)(&liveDumpControl),
      sizeof(liveDumpControl),
      NULL,
      0,
      &returnLength);

  if (!NT_SUCCESS(status)) {
    printf("NtSystemDebugControl failed:  %08x\n", status);
    goto Exit;
  }

Exit:
  return status;
}
