using MauiCrudApp.Common.Controls;
using MauiCrudApp.Common.Interfaces;

namespace MauiCrudApp.Ble.SwiftApp
{
    public partial class AppShell : ShellBase
    {
        public AppShell(IDialogService dialogService) : base(dialogService)
        {
            InitializeComponent();
        }
    }
}
