namespace NorthstarLauncher.Models;

public sealed class ValidationResult
{
    public bool IsValid { get; }
    public string Message { get; }
    public int ExitCode { get; }

    private ValidationResult(bool isValid, string message, int exitCode)
    {
        IsValid = isValid;
        Message = message;
        ExitCode = exitCode;
    }

    public static ValidationResult Success(int exitCode = 0)
    {
        return new ValidationResult(true, string.Empty, exitCode);
    }

    public static ValidationResult Fail(string message)
    {
        return new ValidationResult(false, message, 1);
    }
}
