using System;
using System.Windows;
using System.Windows.Threading;

namespace TransparencyApp
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // Catch unhandled exceptions on background threads
            AppDomain.CurrentDomain.UnhandledException += (s, args) =>
            {
                var ex = (Exception)args.ExceptionObject;
                MessageBox.Show($"Fatal Error:\n{ex.Message}", "Transparency Error", MessageBoxButton.OK, MessageBoxImage.Error);
            };

            // Catch unhandled exceptions on the UI (dispatcher) thread
            DispatcherUnhandledException += (s, args) =>
            {
                MessageBox.Show($"UI Error:\n{args.Exception.Message}", "Transparency Error", MessageBoxButton.OK, MessageBoxImage.Error);
                args.Handled = true;
            };

            base.OnStartup(e);
        }
    }
}
