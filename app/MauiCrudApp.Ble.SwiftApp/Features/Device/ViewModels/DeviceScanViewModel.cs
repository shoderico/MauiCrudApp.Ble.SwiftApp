using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using MauiCrudApp.Ble.Interfaces;
using MauiCrudApp.Common.Interfaces;
using MauiCrudApp.Common.Navigation;
using MauiCrudApp.Common.ViewModels;
using Plugin.BLE.Abstractions.Contracts;

namespace MauiCrudApp.Ble.SwiftApp.Features.Device.ViewModels
{
    public partial class DeviceScanViewModel : ViewModelBase<DeviceScanParameter>
    {
        private readonly INavigationService _navigationService;
        private readonly IDialogService _dialogService;
        private readonly IBleAdapterService _bleAdapterService;
        private readonly IBleDeviceManager _bleDeviceManager;

        public DeviceScanViewModel(
                  INavigationParameterStore parameterStore
                , INavigationService navigationService
                , IDialogService dialogService
                , IBleAdapterService bleAdapterService
                , IBleDeviceManager bleDeviceManager
            ) : base(
                parameterStore
            )
        {
            _navigationService = navigationService;
            _dialogService = dialogService;
            _bleAdapterService = bleAdapterService;
            _bleDeviceManager = bleDeviceManager;


            _bleAdapterService.DeviceDiscovered += (s, e) => MainThread.BeginInvokeOnMainThread(() => Devices.Add(e.Device));
            _bleAdapterService.ScanStateChanged += (s, isScanning) => UpdateScanningState();
        }



        public ObservableCollection<IDevice> Devices { get; } = new();

        [ObservableProperty]
        private bool isScanning;

        [ObservableProperty]
        private bool canStartScan;

        [ObservableProperty]
        private bool canStopScan;



        public override async Task InitializeAsync(DeviceScanParameter parameter, bool isInitialized)
        {
            try
            {
                // start scanning
                await StartScan();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("InitializeAsync: Error", ex.Message, "OK");
            }
        }

        public override async Task FinalizeAsync(bool isFinalized)
        {
            try
            {
                // stop scanning
                await StopScan();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("FinalizeAsync: Error", ex.Message, "OK");
            }
        }



        [RelayCommand]
        private async Task StartScan()
        {
            try
            {
                Devices.Clear();
                await _bleAdapterService.StartScanningAsync();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("StartScan: Error", ex.Message, "OK");
            }
        }

        [RelayCommand]
        private async Task StopScan()
        {
            try
            {
                await _bleAdapterService.StopScanningAsync();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("StartScan: Error", ex.Message, "OK");
            }

        }

        [RelayCommand]
        private async Task Select(IDevice device)
        {
            try
            {
                await _bleAdapterService.StopScanningAsync();
                await _bleDeviceManager.SelectDeviceAsync(device);
                await _navigationService.GoBackAsync();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("Select: Error", ex.Message, "OK");
            }
        }

        [RelayCommand]
        private async Task Cancel()
        {
            try
            {
                await _bleAdapterService.StopScanningAsync();
                await _navigationService.GoBackAsync();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("Cancel: Error", ex.Message, "OK");
            }
        }


        private void UpdateScanningState()
        {
            IsScanning = _bleAdapterService.IsScanning;
            UpdateCommandStates();
        }

        private void UpdateCommandStates()
        {
            CanStartScan = !IsScanning;
            CanStopScan = IsScanning;
        }
    }
}
