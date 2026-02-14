using Directory = System.IO.Directory;
using DirectoryInfo = System.IO.DirectoryInfo;
using File = System.IO.File;
using Path = System.IO.Path;
using NorthstarLauncher.Models;

namespace NorthstarLauncher.Services;

public sealed class LauncherInstallValidator
{
    public ValidationResult Validate(string installDirectory, IReadOnlyList<string> args)
    {
        if (string.IsNullOrWhiteSpace(installDirectory) || !Directory.Exists(installDirectory))
        {
            return ValidationResult.Fail("Failed getting game directory. The game cannot continue and has to exit.");
        }

        string titanfallPath = Path.Combine(installDirectory, "Titanfall2.exe");
        if (!File.Exists(titanfallPath))
        {
            string parentPath = Directory.GetParent(installDirectory)?.FullName ?? string.Empty;
            string grandParentPath = Directory.GetParent(parentPath)?.FullName ?? string.Empty;

            bool isInSubdirectory = (!string.IsNullOrWhiteSpace(parentPath) && File.Exists(Path.Combine(parentPath, "Titanfall2.exe")))
                || (!string.IsNullOrWhiteSpace(grandParentPath) && File.Exists(Path.Combine(grandParentPath, "Titanfall2.exe")));

            if (isInSubdirectory)
            {
                string currentFolder = new DirectoryInfo(installDirectory).Name;
                string aboveFolder = new DirectoryInfo(parentPath).Name;
                return ValidationResult.Fail(
                    "We detected that these files are extracted into a subdirectory of your Titanfall 2 installation. "
                    + $"Move all files from '{currentFolder}' into the game directory just above ('{aboveFolder}').");
            }

            return ValidationResult.Fail(
                "Titanfall2.exe was not found in this directory. Unpack Northstar directly into your Titanfall 2 game installation directory.");
        }

        string retailDirectory = Path.Combine(installDirectory, "bin", "x64_retail");
        if (!Directory.Exists(retailDirectory))
        {
            return ValidationResult.Fail("Missing required directory: bin\\x64_retail.");
        }

        string launcherDllPath = Path.Combine(retailDirectory, "launcher.dll");
        if (!File.Exists(launcherDllPath))
        {
            return ValidationResult.Fail("Missing required file: bin\\x64_retail\\launcher.dll.");
        }

        string tier0Path = Path.Combine(retailDirectory, "tier0.dll");
        if (!File.Exists(tier0Path))
        {
            return ValidationResult.Fail("Missing required file: bin\\x64_retail\\tier0.dll.");
        }

        if (ShouldLoadNorthstar(args, installDirectory))
        {
            string profile = GetProfileFromArgs(args);
            string profileNorthstar = Path.Combine(installDirectory, profile, "Northstar.dll");
            string rootNorthstar = Path.Combine(installDirectory, "Northstar.dll");
            if (!File.Exists(profileNorthstar) && !File.Exists(rootNorthstar))
            {
                return ValidationResult.Fail(
                    $"Northstar.dll was not found in either '{profile}' or the root game directory.");
            }
        }

        return ValidationResult.Success();
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

    private static string GetProfileFromArgs(IReadOnlyList<string> args)
    {
        foreach (string arg in args)
        {
            if (!arg.StartsWith("-profile=", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string value = arg[9..].Trim();
            if (string.IsNullOrWhiteSpace(value))
            {
                break;
            }

            return value.Trim('"');
        }

        return "R2Northstar";
    }
}
