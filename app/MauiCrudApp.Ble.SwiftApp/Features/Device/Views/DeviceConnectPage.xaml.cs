using MauiCrudApp.Common.Views;
using MauiCrudApp.Ble.SwiftApp.Features.Device.ViewModels;

namespace MauiCrudApp.Ble.SwiftApp.Features.Device.Views
{
    public partial class DeviceConnectPage : PageBase
    {
        public DeviceConnectPage(DeviceConnectViewModel viewModel) : base(viewModel)
        {
            InitializeComponent();
            BindingContext = viewModel;
        }
    }
}