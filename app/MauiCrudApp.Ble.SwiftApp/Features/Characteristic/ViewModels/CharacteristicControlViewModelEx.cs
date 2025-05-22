using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using MauiCrudApp.Ble.Interfaces;
using MauiCrudApp.Common.Interfaces;
using MauiCrudApp.Common.Navigation;

namespace MauiCrudApp.Ble.SwiftApp.Features.Characteristic.ViewModels;

public partial class CharacteristicControlViewModelEx : CharacteristicControlViewModel
{
    private readonly IBleDeviceManager _bleDeviceManager;
    private readonly IDialogService _dialogService;
    private readonly CharacteristicStateStore _characteristicStateStore;

    private IBleCharacteristic _writeCharacteristic;

    private readonly System.Timers.Timer _writeTimer;
    private static uint counter = 0;

    public CharacteristicControlViewModelEx(
        INavigationParameterStore parameterStore
        , INavigationService navigationService
        , IDialogService dialogService
        , IBleDeviceManager bleDeviceManager
        , CharacteristicStateStore characteristicStateStore
        ) : base(parameterStore, navigationService, dialogService, bleDeviceManager, characteristicStateStore)
    {
        _bleDeviceManager = bleDeviceManager;
        _dialogService = dialogService;
        _characteristicStateStore = characteristicStateStore;
        _writeCharacteristic = null;

        Interval = 30;
        _writeTimer = new System.Timers.Timer();
        _writeTimer.Elapsed += async (s,e) => await OnWriteTimerElapsedAsync();
        _writeTimer.AutoReset = true;
        CanStartWriting = true;
        CanStopWriting = false;
    }


    public async override Task InitializeAsync(CharacteristicControlParameter parameter, bool isInitialized)
    {
        await base.InitializeAsync(parameter, isInitialized);

        if (_bleDeviceManager.BleDevice.ConnectionState == BleDeviceConnectionState.Connected)
        {
            _writeCharacteristic = null;
            var services = _bleDeviceManager.BleDevice.Services.ToArray();
            foreach (var service in services)
            {
                foreach (var characteristic in service.Characteristics)
                {
                    if (characteristic.Id == Guid.Parse("{6e400003-b5a3-93f3-e0a9-e50e24dcca9e}"))
                    {
                        _writeCharacteristic = characteristic;
                        break;
                    }
                }

                if (_writeCharacteristic != null)
                    break;
            }
        }
    }

    private async Task OnWriteTimerElapsedAsync()
    {
        if (_writeCharacteristic == null)
            return;

        try
        {

            await MainThread.InvokeOnMainThreadAsync(() =>
            {
                // 10 bytes dummy data
                byte[] data = new byte[10];
                data[0] = (byte)(counter & 0xFF);
                data[1] = (byte)((counter >> 8) & 0xFF);
                counter++;

                try
                {
                    _writeCharacteristic.WriteAsync(data);
                }
                catch (Exception ex)
                {
                    StopWriting();
                    _dialogService.DisplayAlert("Error", $"Write failed: {ex.Message}", "OK");
                }

            });

        }
        catch (Exception ex)
        {
            await StopWriting();
            await _dialogService.DisplayAlert("Error", $"Write failed: {ex.Message}", "OK");
        }
    }

    [ObservableProperty]
    private bool canStartWriting;

    [ObservableProperty]
    private bool canStopWriting;

    [ObservableProperty]
    private int interval;

    [RelayCommand]
    private Task StartWriting()
    {
        _writeTimer.Interval = Interval;
        _writeTimer.Start();
        CanStartWriting = false;
        CanStopWriting = true;
        return Task.CompletedTask;
    }

    [RelayCommand]
    private Task StopWriting()
    {
        _writeTimer.Stop();
        CanStartWriting = true;
        CanStopWriting = false;
        return Task.CompletedTask;
    }

    [RelayCommand]
    private Task ResetMaxBps()
    {
        _characteristicStateStore.ResetAllMaxBps();
        return Task.CompletedTask;
    }
}
