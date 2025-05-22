using CommunityToolkit.Mvvm.Input;
using MauiCrudApp.Ble.Interfaces;
using MauiCrudApp.Common.Interfaces;
using MauiCrudApp.Common.Navigation;
using MauiCrudApp.Common.ViewModels;
using MauiCrudApp.Ble.SwiftApp.Features.Device.Views;

namespace MauiCrudApp.Ble.SwiftApp.Features.Device.ViewModels
{
    public partial class DeviceConnectViewModel : ViewModelBase<DeviceConnectParameter>
    {
        private readonly INavigationService _navigationService;
        private readonly IDialogService _dialogService;
        private readonly IBlePlatformService _blePlatformService;
        private readonly IBleDeviceManager _bleDeviceManager;

        public DeviceConnectViewModel(
                  INavigationParameterStore parameterStore
                , INavigationService navigationService
                , IDialogService dialogService
                , IBlePlatformService blePlatformService
                , IBleDeviceManager bleDeviceManager
            ) : base(
                parameterStore
            )
        {
            _navigationService = navigationService;
            _dialogService = dialogService;
            _blePlatformService = blePlatformService;
            _bleDeviceManager = bleDeviceManager;
        }


        public IBleDevice SelectedBleDevice => _bleDeviceManager.BleDevice;


        public override async Task InitializeAsync(DeviceConnectParameter parameter, bool isInitialized)
        {
            try
            {
                if (!isInitialized)
                {
                    if (!await _blePlatformService.CheckBluetoothPermissionAsync())
                    {
                        // We cannot use Bluetooth... but do nothing for now
                    }
                }
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("InitializeAsync: Error", ex.Message, "OK");
            }
        }



        [RelayCommand]
        private async Task Scan()
        {
            try
            {
                var parameter = new DeviceScanParameter();
                await _navigationService.PushAsync(typeof(DeviceScanPage), parameter);
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("Scan: Error", ex.Message, "OK");
            }
        }

        [RelayCommand]
        private async Task Connect()
        {
            if (_bleDeviceManager.BleDevice.Id != Guid.Empty)
                await _bleDeviceManager.ConnectAsync();
        }

        [RelayCommand]
        private async Task Disconnect()
        {
            if (_bleDeviceManager.BleDevice.ConnectionState == BleDeviceConnectionState.Connected)
                await _bleDeviceManager.DisconnectAsync();
        }

        [RelayCommand]
        private async Task Cancel()
        {
            if (_bleDeviceManager.BleDevice.ConnectionState == BleDeviceConnectionState.Connecting)
                await _bleDeviceManager.CancelConnectingAsync();
        }

        [RelayCommand]
        private async Task Reset()
        {
            await _bleDeviceManager.ResetDeviceAsync();
        }
    }
}
