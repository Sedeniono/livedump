# Introduction
Newer Windows versions allow the creation of two specific types of kernel dumps from user mode:
* Kernel triage dumps: These are taken for a specific process, and contain the kernel portion of the callstack. Available roughly since Windows Vista.
* Kernel live dumps: These are not limited to a certain process. Available since Windows 8.1.

There are various tools and APIs available, that allow to create these dumps. In the end, they eventually use the undocumented `NtSystemDebugControl()` API, with the parameters.
* `SysDbgGetTriageDump=29` for kernel triage dumps.
* `SysDbgGetLiveKernelDump=37` for kernel live dumps.

The code in this repository here shows how to call that API directly.

This is a fork and improved version of [github.com/lilhoser/livedump](https://github.com/lilhoser/livedump).


# Synopsis
```
LiveDump.exe [type] [options] <FileName>
Type:
        triage : create a triage dump (parameter 29)
        kernel : create a kernel dump (parameter 37)
Options (triage dump only):
        -t : Number of threads to include in the dump. Should be between 1 and 16. Default is 4.
        -p : PID to dump
Options (kernel dump only):
        -c : compress memory pages in dump
        -d : Use dump stack (not implemented in Windows 8.1)
        -h : add hypervisor pages
        -u : also dump user space memory
FileName is the full path to the dump file to create.
```

Run from an elevated command prompt.
The resulting dump files can be opened in WinDbg.

Note: If a triage dump seems to be incomplete, try a smaller thread number (`-t`).

Example: Create a triage dump for process with PID 12345, including the first 3 threads:
```
.\LiveDump triage -p 12345 -t 3 TriageDump.Kernel.dmp
```


# More information

## Documented APIs
The WinAPI [`MiniDumpWriteDump()`](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump) allows the creation of "kernel mini dumps" (aka kernel triage dumps) via [`WriteKernelMinidumpCallback`](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/ne-minidumpapiset-minidump_callback_type).
From my research, this is calling `NtSystemDebugControl()` with parameter `SysDbgGetTriageDump=29`.


## Other tools
Other tools that can capture kernel triage dumps:
* [Sysinternals `procdump`](https://learn.microsoft.com/en-us/sysinternals/downloads/procdump): The call `procdump -mk` creates a kernel triage dump. Apparently uses `MiniDumpWriteDump()`.
* [System Informer](https://github.com/winsiderss/systeminformer): Uses `MiniDumpWriteDump()` [according to the source](https://github.com/winsiderss/systeminformer/blob/edc23a5c62a7dd60bcd72ddda53af96838b908a2/SystemInformer/mdump.c#L361). System Informer can be instructed to create the kernel triage dump by setting `EnableMinidumpKernelMinidump` to 1 in its XML config file.
* [`KDbgCtrl`](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/kdbgctrl-command-line-options): The [Debugging Tools for Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools) comes with the tool `KDbgCtrl`, where the [`-td`](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/kdbgctrl-command-line-options) parameter allows the creation of a kernel triage dump.

Other tools that can capture kernel live dumps:
* [Windows Task Manager](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/task-manager-live-dump) (since Windows 11 22H2)
* [System Informer](https://github.com/winsiderss/systeminformer): Uses [`NtSystemDebugControl()` with `SysDbgGetLiveKernelDump=37`](https://github.com/winsiderss/systeminformer/blob/82c625783a035fa7eac355783f527bb53fb1a384/SystemInformer/kdump.c#L75).
* [Sysinternals `livekd`](https://learn.microsoft.com/en-us/sysinternals/downloads/livekd)


## Resources
Links to interesting resources:
* [http://gary-nebbett.blogspot.com/2016/04/examining-windows-kernel-mode-stacks.html](http://gary-nebbett.blogspot.com/2016/04/examining-windows-kernel-mode-stacks.html)
* [http://www.nynaeve.net/?p=298](http://www.nynaeve.net/?p=298)
* [https://crashdmp.wordpress.com/2014/08/04/livedump-1-0-is-available/](https://crashdmp.wordpress.com/2014/08/04/livedump-1-0-is-available/)
* [https://blog.slowerzs.net/posts/pplsystem/](https://blog.slowerzs.net/posts/pplsystem/)
* [https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/kernel-live-dump-code-reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/kernel-live-dump-code-reference)
