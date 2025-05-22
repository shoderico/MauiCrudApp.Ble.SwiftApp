using MauiCrudApp.Common.Views;
using MauiCrudApp.Ble.SwiftApp.Features.Device.ViewModels;

namespace MauiCrudApp.Ble.SwiftApp.Features.Device.Views
{
    public partial class DeviceScanPage : PageBase
    {
        public DeviceScanPage(DeviceScanViewModel viewModel) : base(viewModel)
        {
            InitializeComponent();
            BindingContext = viewModel;
        }
    }
}