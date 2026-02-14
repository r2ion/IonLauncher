using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Threading;
using Directory = System.IO.Directory;
using DirectoryInfo = System.IO.DirectoryInfo;
using File = System.IO.File;
using Path = System.IO.Path;
using Microsoft.Win32;
using NorthstarLauncher.Models;

namespace NorthstarLauncher.Services;

[SupportedOSPlatform("windows")]
public sealed class GameLaunchService
{
    public ValidationResult Launch(string installDirectory, IReadOnlyList<string> args)
    {
        try
        {
            return LaunchInternal(installDirectory, args);
        }
        catch (Exception ex)
        {
            return ValidationResult.Fail($"Failed to launch game: {ex.Message}");
        }
    }

    private static ValidationResult LaunchInternal(string installDirectory, IReadOnlyList<string> args)
    {
        if (args.Any(arg => arg.Equals("-waitfordebugger", StringComparison.OrdinalIgnoreCase)))
        {
            while (!NativeMethods.IsDebuggerPresent())
            {
                Thread.Sleep(100);
            }
        }

        if (!NativeMethods.SetEnvironmentVariable("OPENSSL_ia32cap", "~0x200000200000000"))
        {
            return ValidationResult.Fail("Failed to set OPENSSL_ia32cap environment variable.");
        }

        if (!NativeMethods.SetCurrentDirectory(installDirectory))
        {
            return ValidationResult.Fail("Failed setting current directory to the game path.");
        }

        bool noOriginStartup = args.Any(arg => arg.Equals("-noOriginStartup", StringComparison.OrdinalIgnoreCase));

        if (!noOriginStartup)
        {
            ValidationResult originResult = EnsureOriginStarted();
            if (!originResult.IsValid)
            {
                return originResult;
            }
        }

        PrependRetailPath(installDirectory);
        EnsureStartupArgFiles(installDirectory);

        string retailDirectory = Path.Combine(installDirectory, "bin", "x64_retail");
        string tier0Path = Path.Combine(retailDirectory, "tier0.dll");
        nint tier0Module = TryLoadLibraryWithError(tier0Path, "tier0.dll", installDirectory, out ValidationResult? tier0Error);
        if (tier0Module == nint.Zero)
        {
            return tier0Error!;
        }

        if (ShouldLoadNorthstar(args, installDirectory))
        {
            ValidationResult northstarResult = TryLoadNorthstar(installDirectory, args);
            if (!northstarResult.IsValid)
            {
                return northstarResult;
            }
        }

        string launcherPath = Path.Combine(retailDirectory, "launcher.dll");
        nint launcherModule = TryLoadLibraryWithError(launcherPath, "launcher.dll", installDirectory, out ValidationResult? launcherError);
        if (launcherModule == nint.Zero)
        {
            return launcherError!;
        }

        nint launcherMainAddress = NativeMethods.GetProcAddress(launcherModule, "LauncherMain");
        if (launcherMainAddress == nint.Zero)
        {
            return ValidationResult.Fail("Failed to resolve LauncherMain in launcher.dll.");
        }

        var launcherMain = Marshal.GetDelegateForFunctionPointer<LauncherMainDelegate>(launcherMainAddress);
        int exitCode = launcherMain(nint.Zero, nint.Zero, nint.Zero, 0);
        return ValidationResult.Success(exitCode);
    }

    private static ValidationResult TryLoadNorthstar(string installDirectory, IReadOnlyList<string> args)
    {
        string profile = GetProfile(args);
        string profilePath = Path.Combine(installDirectory, profile, "Northstar.dll");
        string rootPath = Path.Combine(installDirectory, "Northstar.dll");
        string dllPath = File.Exists(profilePath) ? profilePath : rootPath;

        nint module = TryLoadLibraryWithError(dllPath, "Northstar.dll", installDirectory, out ValidationResult? loadError);
        if (module == nint.Zero)
        {
            return loadError!;
        }

        nint initAddress = NativeMethods.GetProcAddress(module, "InitialiseNorthstar");
        if (initAddress == nint.Zero)
        {
            return ValidationResult.Fail("Failed to resolve InitialiseNorthstar in Northstar.dll.");
        }

        var init = Marshal.GetDelegateForFunctionPointer<InitialiseNorthstarDelegate>(initAddress);
        bool initialized = init();
        return initialized
            ? ValidationResult.Success()
            : ValidationResult.Fail("InitialiseNorthstar reported failure.");
    }

    private static bool ShouldLoadNorthstar(IReadOnlyList<string> args, string installDirectory)
    {
        if (args.Any(arg => arg.Equals("-nonorthstardll", StringComparison.OrdinalIgnoreCase)))
        {
            return false;
        }

        string runNorthstarPath = Path.Combine(installDirectory, "run_northstar.txt");
        if (!File.Exists(runNorthstarPath))
        {
            return true;
        }

        string content = File.ReadAllText(runNorthstarPath);
        return !content.StartsWith("0", StringComparison.Ordinal);
    }

    private static void EnsureStartupArgFiles(string installDirectory)
    {
        string startupArgs = Path.Combine(installDirectory, "ns_startup_args.txt");
        if (!File.Exists(startupArgs))
        {
            File.WriteAllText(startupArgs, "-multiple");
        }
    }

    private static void PrependRetailPath(string installDirectory)
    {
        string? path = Environment.GetEnvironmentVariable("PATH");
        string retailPath = Path.Combine(installDirectory, "bin", "x64_retail");
        string updatedPath = string.IsNullOrWhiteSpace(path)
            ? retailPath
            : $"{retailPath};{path}";
        Environment.SetEnvironmentVariable("PATH", updatedPath);
    }

    private static ValidationResult EnsureOriginStarted()
    {
        if (IsProcessRunning("Origin") || IsProcessRunning("EADesktop"))
        {
            return ValidationResult.Success();
        }

        string? originPath = GetOriginPathFromRegistry();
        if (string.IsNullOrWhiteSpace(originPath) || !File.Exists(originPath))
        {
            return ValidationResult.Fail("Error: failed reading Origin path.");
        }

        Process.Start(new ProcessStartInfo
        {
            FileName = originPath,
            UseShellExecute = false,
            WindowStyle = ProcessWindowStyle.Minimized
        });

        var timeout = TimeSpan.FromSeconds(30);
        DateTime start = DateTime.UtcNow;
        while (!IsProcessRunning("OriginClientService") && !IsProcessRunning("EADesktop"))
        {
            if (DateTime.UtcNow - start > timeout)
            {
                return ValidationResult.Fail("Timed out waiting for Origin/EA Desktop to start.");
            }

            Thread.Sleep(500);
        }

        return AwaitOriginStartup();
    }

    private static ValidationResult AwaitOriginStartup()
    {
        try
        {
            using var client = new TcpClient();
            client.Connect(IPAddress.Loopback, 3216);
            using NetworkStream stream = client.GetStream();
            stream.ReadTimeout = 45000;

            byte[] buffer = new byte[4096];
            DateTime deadline = DateTime.UtcNow.AddSeconds(60);

            while (DateTime.UtcNow < deadline)
            {
                int read = stream.Read(buffer, 0, buffer.Length);
                if (read <= 0)
                {
                    Thread.Sleep(250);
                    continue;
                }

                string chunk = System.Text.Encoding.UTF8.GetString(buffer, 0, read);
                if (chunk.Contains("<LSX>", StringComparison.Ordinal))
                {
                    Thread.Sleep(8000);
                    return ValidationResult.Success();
                }
            }

            return ValidationResult.Fail("Timed out waiting for LSX startup response.");
        }
        catch
        {
            return ValidationResult.Success();
        }
    }

    private static bool IsProcessRunning(string processName)
    {
        return Process.GetProcessesByName(processName).Length > 0;
    }

    private static string? GetOriginPathFromRegistry()
    {
        using RegistryKey? key = Registry.LocalMachine.OpenSubKey("SOFTWARE\\WOW6432Node\\Origin");
        return key?.GetValue("ClientPath") as string;
    }

    private static nint TryLoadLibraryWithError(string path, string libraryName, string installDirectory, out ValidationResult? error)
    {
        nint module = NativeMethods.LoadLibraryEx(path, nint.Zero, NativeMethods.LoadWithAlteredSearchPath);
        if (module != nint.Zero)
        {
            error = null;
            return module;
        }

        int win32Error = Marshal.GetLastWin32Error();
        string message = BuildLibraryErrorMessage(win32Error, libraryName, path, installDirectory);
        error = ValidationResult.Fail(message);
        return nint.Zero;
    }

    private static string BuildLibraryErrorMessage(int errorCode, string libraryName, string location, string installDirectory)
    {
        string message =
            $"Failed to load {libraryName} at '{location}' ({errorCode}). Make sure Northstar was installed correctly.";

        if (errorCode == 126 && File.Exists(location))
        {
            return message
                + "\n\nThe file exists, so one of its dependencies is likely missing. "
                + "Try installing VC++ 2022 Redistributable: https://aka.ms/vs/17/release/vc_redist.x64.exe and repairing game files.";
        }

        string titanfallInCurrent = Path.Combine(installDirectory, "Titanfall2.exe");
        string parent = Directory.GetParent(installDirectory)?.FullName ?? string.Empty;
        string grandParent = Directory.GetParent(parent)?.FullName ?? string.Empty;

        bool currentHasTitanfall = File.Exists(titanfallInCurrent);
        bool parentHasTitanfall = !string.IsNullOrWhiteSpace(parent) && File.Exists(Path.Combine(parent, "Titanfall2.exe"));
        bool grandParentHasTitanfall = !string.IsNullOrWhiteSpace(grandParent) && File.Exists(Path.Combine(grandParent, "Titanfall2.exe"));

        if (!currentHasTitanfall && (parentHasTitanfall || grandParentHasTitanfall))
        {
            string currentName = new DirectoryInfo(installDirectory).Name;
            string aboveName = new DirectoryInfo(parent).Name;
            return message
                + $"\n\nFiles appear to be in a subdirectory. Move everything from '{currentName}' to '{aboveName}'.";
        }

        if (!currentHasTitanfall)
        {
            return message
                + "\n\nTitanfall2.exe is missing in the current directory. Unpack Northstar into the game installation folder.";
        }

        return message
            + "\n\nTitanfall2.exe is present. Installation may be incomplete or corrupted.";
    }

    private static string GetProfile(IReadOnlyList<string> args)
    {
        foreach (string arg in args)
        {
            if (!arg.StartsWith("-profile=", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string value = arg[9..].Trim();
            if (!string.IsNullOrWhiteSpace(value))
            {
                return value.Trim('"');
            }
        }

        return "R2Northstar";
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int LauncherMainDelegate(nint hInstance, nint hPrevInstance, nint commandLine, int showCmd);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate bool InitialiseNorthstarDelegate();
}
