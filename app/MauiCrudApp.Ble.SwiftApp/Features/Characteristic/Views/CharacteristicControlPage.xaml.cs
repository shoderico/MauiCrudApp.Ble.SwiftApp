using MauiCrudApp.Common.Views;
using MauiCrudApp.Ble.SwiftApp.Features.Characteristic.ViewModels;

namespace MauiCrudApp.Ble.SwiftApp.Features.Characteristic.Views
{
    public partial class CharacteristicControlPage : PageBase
    {
        public CharacteristicControlPage(CharacteristicControlViewModelEx viewModel) : base(viewModel)
        {
            InitializeComponent();
            BindingContext = viewModel;
        }
    }
}