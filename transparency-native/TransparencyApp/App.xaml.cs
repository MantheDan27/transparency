using System;
using System.Windows;

namespace TransparencyApp
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            AppDomain.CurrentDomain.UnhandledException += (s, args) => {
                var ex = (Exception)args.ExceptionObject;
                MessageBox.Show($"CRITICAL ERROR: {ex.Message}\n\n{ex.StackTrace}", "Transparency Fatal Error", MessageBoxButton.OK, MessageBoxImage.Error);
            };

            base.OnStartup(e);
        }
    }
}
