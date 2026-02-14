using System.Windows;
using NorthstarLauncher.Models;
using NorthstarLauncher.Services;

namespace NorthstarLauncher
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            var installDirectory = AppContext.BaseDirectory;
            var commandLineArgs = Environment.GetCommandLineArgs().Skip(1).ToArray();

            var validator = new LauncherInstallValidator();
            ValidationResult validationResult = validator.Validate(installDirectory, commandLineArgs);

            if (!validationResult.IsValid)
            {
                ShowValidationWindow(validationResult.Message);
                return;
            }

            var launcher = new GameLaunchService();
            ValidationResult launchResult = launcher.Launch(installDirectory, commandLineArgs);
            if (launchResult.IsValid)
            {
                Shutdown(launchResult.ExitCode);
                return;
            }

            ShowValidationWindow(launchResult.Message);
        }

        private void ShowValidationWindow(string message)
        {
            var window = new MainWindow(message);
            MainWindow = window;
            window.Show();
        }
    }
}
