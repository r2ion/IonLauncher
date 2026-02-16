using System.Windows;
using NorthstarLauncher.Services;

namespace NorthstarLauncher
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            base.OnStartup(e);

            var installDirectory = AppContext.BaseDirectory;
            var commandLineArgs = Environment.GetCommandLineArgs().Skip(1).ToArray();

            var launcher = new GameLaunchService();
            var launchResult = launcher.Launch(installDirectory, commandLineArgs);
            if (launchResult.IsValid)
            {
                Shutdown(launchResult.ExitCode);
                return;
            }

            ShowLauncherWindow();
        }

        private void ShowLauncherWindow()
        {
            var window = new MainWindow();
            MainWindow = window;
            ShutdownMode = ShutdownMode.OnMainWindowClose;
            window.Show();
            window.Activate();
        }
    }
}
