using System.Windows;

namespace NorthstarLauncher
{
    public partial class MainWindow : Window
    {
        public string ErrorMessage { get; }

        public MainWindow()
            : this("Northstar installation is invalid or incomplete. Use this launcher window to repair or install files.")
        {
        }

        public MainWindow(string? errorMessage)
        {
            ErrorMessage = string.IsNullOrWhiteSpace(errorMessage)
                ? "Northstar installation is invalid or incomplete. Use this launcher window to repair or install files."
                : errorMessage;
            InitializeComponent();
            DataContext = this;
        }

        private void OnCloseClicked(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
