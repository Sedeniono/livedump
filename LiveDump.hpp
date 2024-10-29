#pragma once

#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include <winternl.h>
#include <TlHelp32.h>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define TRIAGE_SIZE 0x100000 // must be >132kB and <1MB

#pragma comment(lib, "ntdll")


// Taken from https://github.com/winsiderss/systeminformer/blob/82c625783a035fa7eac355783f527bb53fb1a384/phnt/include/ntexapi.h#L4654
typedef enum _SYSDBG_COMMAND
{
  SysDbgQueryModuleInformation,
  SysDbgQueryTraceInformation,
  SysDbgSetTracepoint,
  SysDbgSetSpecialCall, // PVOID
  SysDbgClearSpecialCalls, // void
  SysDbgQuerySpecialCalls,
  SysDbgBreakPoint,
  SysDbgQueryVersion, // DBGKD_GET_VERSION64
  SysDbgReadVirtual, // SYSDBG_VIRTUAL
  SysDbgWriteVirtual, // SYSDBG_VIRTUAL
  SysDbgReadPhysical, // SYSDBG_PHYSICAL // 10
  SysDbgWritePhysical, // SYSDBG_PHYSICAL
  SysDbgReadControlSpace, // SYSDBG_CONTROL_SPACE
  SysDbgWriteControlSpace, // SYSDBG_CONTROL_SPACE
  SysDbgReadIoSpace, // SYSDBG_IO_SPACE
  SysDbgWriteIoSpace, // SYSDBG_IO_SPACE
  SysDbgReadMsr, // SYSDBG_MSR
  SysDbgWriteMsr, // SYSDBG_MSR
  SysDbgReadBusData, // SYSDBG_BUS_DATA
  SysDbgWriteBusData, // SYSDBG_BUS_DATA
  SysDbgCheckLowMemory, // 20
  SysDbgEnableKernelDebugger,
  SysDbgDisableKernelDebugger,
  SysDbgGetAutoKdEnable,
  SysDbgSetAutoKdEnable,
  SysDbgGetPrintBufferSize,
  SysDbgSetPrintBufferSize,
  SysDbgGetKdUmExceptionEnable,
  SysDbgSetKdUmExceptionEnable,
  SysDbgGetTriageDump, // SYSDBG_TRIAGE_DUMP, CONTROL_TRIAGE_DUMP
  SysDbgGetKdBlockEnable, // 30
  SysDbgSetKdBlockEnable,
  SysDbgRegisterForUmBreakInfo,
  SysDbgGetUmBreakPid,
  SysDbgClearUmBreakPid,
  SysDbgGetUmAttachPid,
  SysDbgClearUmAttachPid,
  SysDbgGetLiveKernelDump, // SYSDBG_LIVEDUMP_CONTROL, CONTROL_KERNEL_DUMP
  SysDbgKdPullRemoteFile, // SYSDBG_KD_PULL_REMOTE_FILE
  SysDbgMaxInfoClass
} SYSDBG_COMMAND;


//
// From NDK, argument required for parameter 29.
//
typedef struct _SYSDBG_TRIAGE_DUMP
{
  ULONG Flags;
  ULONG BugCheckCode;
  ULONG_PTR BugCheckParam1;
  ULONG_PTR BugCheckParam2;
  ULONG_PTR BugCheckParam3;
  ULONG_PTR BugCheckParam4;
  ULONG ProcessHandles;
  ULONG ThreadHandles;
  PHANDLE Handles;
} SYSDBG_TRIAGE_DUMP, *PSYSDBG_TRIAGE_DUMP;


//
// Undocumented.  Structures relevant for new parameter 37.
// Greetz to Alex I.
//
typedef union _SYSDBG_LIVEDUMP_CONTROL_FLAGS
{
  struct
  {
    ULONG UseDumpStorageStack : 1;
    ULONG CompressMemoryPagesData : 1;
    ULONG IncludeUserSpaceMemoryPages : 1;
    ULONG Reserved : 29;
  };
  ULONG AsUlong;
} SYSDBG_LIVEDUMP_CONTROL_FLAGS;


typedef union _SYSDBG_LIVEDUMP_CONTROL_ADDPAGES
{
  struct
  {
    ULONG HypervisorPages : 1;
    ULONG Reserved : 31;
  };
  ULONG AsUlong;
} SYSDBG_LIVEDUMP_CONTROL_ADDPAGES;


typedef struct _SYSDBG_LIVEDUMP_CONTROL
{
  ULONG Version;
  ULONG BugCheckCode;
  ULONG_PTR BugCheckParam1;
  ULONG_PTR BugCheckParam2;
  ULONG_PTR BugCheckParam3;
  ULONG_PTR BugCheckParam4;
  PVOID DumpFileHandle;
  PVOID CancelEventHandle;
  SYSDBG_LIVEDUMP_CONTROL_FLAGS Flags;
  SYSDBG_LIVEDUMP_CONTROL_ADDPAGES AddPagesControl;
} SYSDBG_LIVEDUMP_CONTROL, *PSYSDBG_LIVEDUMP_CONTROL;


typedef NTSTATUS(__stdcall * NtSystemDebugControl)(
    ULONG ControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnLength);


BOOL EnablePrivilege(__in PCWSTR PrivilegeName, __in BOOLEAN Acquire);


NTSTATUS
CreateTriageDump(__in HANDLE FileHandle, __in ULONG Pid, __in ULONG numThreads);


NTSTATUS
CreateKernelDump(
    __in HANDLE FileHandle,
    __in SYSDBG_LIVEDUMP_CONTROL_FLAGS Flags,
    __in SYSDBG_LIVEDUMP_CONTROL_ADDPAGES Pages);


INT wmain(__in INT Argc, __in PWCHAR Argv[]);
