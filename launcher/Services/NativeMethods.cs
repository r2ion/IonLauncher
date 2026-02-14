using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace NorthstarLauncher.Services;

[SupportedOSPlatform("windows")]
internal static class NativeMethods
{
    internal const uint LoadWithAlteredSearchPath = 0x00000008;

    [DllImport("kernel32.dll", EntryPoint = "LoadLibraryExW", CharSet = CharSet.Unicode, SetLastError = true)]
    internal static extern nint LoadLibraryEx(string fileName, nint file, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool FreeLibrary(nint module);

    [DllImport("kernel32.dll", SetLastError = true)]
    internal static extern nint GetProcAddress(nint module, [MarshalAs(UnmanagedType.LPStr)] string procName);

    [DllImport("kernel32.dll", EntryPoint = "GetModuleHandleW", CharSet = CharSet.Unicode, SetLastError = true)]
    internal static extern nint GetModuleHandle(string moduleName);

    [DllImport("kernel32.dll", EntryPoint = "SetCurrentDirectoryW", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool SetCurrentDirectory(string pathName);

    [DllImport("kernel32.dll", EntryPoint = "SetEnvironmentVariableW", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool SetEnvironmentVariable(string name, string value);

    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool IsDebuggerPresent();
}
