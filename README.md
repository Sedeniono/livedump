Improved version of [github.com/lilhoser/livedump](https://github.com/lilhoser/livedump).

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
        -d : Use dump stack (currently not implemented in Windows 8.1, 9600.16404.x86fre.winblue_gdr.130913-2141)
        -h : add hypervisor pages
        -u : also dump user space memory
FileName is the full path to the dump file to create.
```

Run from an elevated command prompt.

The resulting dump files can be opened in WinDbg.

Example: Create a triage dump for process with PID 12345, including the first 3 threads:
```
.\LiveDump triage -p 12345 -t 3 TriageDump.Kernel.dmp
```
