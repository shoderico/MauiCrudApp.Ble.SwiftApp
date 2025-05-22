using System.Collections.ObjectModel;
using System.Timers;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using MauiCrudApp.Ble.Interfaces;
using MauiCrudApp.Common.Interfaces;
using Plugin.BLE.Abstractions;

namespace MauiCrudApp.Ble.SwiftApp.Features.Characteristic.ViewModels
{
    public partial class CharacteristicViewModel : ObservableObject
    {
        private readonly IBleDeviceManager _bleDeviceManager;
        private readonly IDialogService _dialogService;
        private readonly Guid _serviceId;



        #region bps

        private readonly Dictionary<BleValueChangeSource, long> _bytesTransferred;
        private readonly Dictionary<BleValueChangeSource, long> _lastBytesTransferred;
        private readonly Dictionary<BleValueChangeSource, double> _maxBps;

        [ObservableProperty]
        private double writeBps;
        [ObservableProperty]
        private double readBps;
        [ObservableProperty]
        private double notifyBps;

        [ObservableProperty]
        private double maxWriteBps;
        [ObservableProperty]
        private double maxReadBps;
        [ObservableProperty]
        private double maxNotifyBps;

        private readonly System.Timers.Timer _bpsTimer;

        #endregion



        [ObservableProperty]
        private IBleCharacteristic characteristic;

        [ObservableProperty]
        private string writeValue;

        [ObservableProperty]
        private ObservableCollection<BleValueChangedEventArgs> readValues;

        [ObservableProperty]
        private ObservableCollection<CharacteristicWriteType> writeTypeOptions;

        public CharacteristicViewModel(IBleCharacteristic characteristic, Guid serviceId, IBleDeviceManager bleDeviceManager, IDialogService dialogService)
        {
            Characteristic = characteristic;
            _serviceId = serviceId;
            _bleDeviceManager = bleDeviceManager;
            _dialogService = dialogService;
            WriteValue = string.Empty;
            ReadValues = new ObservableCollection<BleValueChangedEventArgs>();

            WriteTypeOptions = new ObservableCollection<CharacteristicWriteType>
            {
                CharacteristicWriteType.Default,
                CharacteristicWriteType.WithResponse,
                CharacteristicWriteType.WithoutResponse
            };



            #region bps

            // initialize Dictionary
            _bytesTransferred = new Dictionary<BleValueChangeSource, long>
            {
                { BleValueChangeSource.Read, 0 },
                { BleValueChangeSource.Write, 0 },
                { BleValueChangeSource.Notify, 0 }
            };
            _lastBytesTransferred = new Dictionary<BleValueChangeSource, long>
            {
                { BleValueChangeSource.Read, 0 },
                { BleValueChangeSource.Write, 0 },
                { BleValueChangeSource.Notify, 0 }
            };
            _maxBps = new Dictionary<BleValueChangeSource, double>
            {
                { BleValueChangeSource.Read, 0 },
                { BleValueChangeSource.Write, 0 },
                { BleValueChangeSource.Notify, 0 }
            };

            // initialize bps and max bps
            WriteBps = ReadBps = NotifyBps = 0;
            MaxWriteBps = MaxReadBps = MaxNotifyBps = 0;

            // timers
            _bpsTimer = new System.Timers.Timer(1000); // 1 seconds
            _bpsTimer.Elapsed += CalculateBps;
            _bpsTimer.AutoReset = true;
            _bpsTimer.Start();

            #endregion



            // Subscribe to value changes
            characteristic.ValueChanged += async (s, value) =>
            {
                // record the bytes
                if (value.Value != null)
                {
                    _bytesTransferred[value.Source] += value.Value.Length;
                    //Console.WriteLine($"ValueChanged: Source={value.Source}, Bytes={value.Value.Length}, Total={_bytesTransferred[value.Source]}");
                }


                if (MainThread.IsMainThread)
                {
                    OnPropertyChanged(nameof(Characteristic));
                }
                else
                {
                    await MainThread.InvokeOnMainThreadAsync(() =>
                    {
                        OnPropertyChanged(nameof(Characteristic));
                    });
                }

                if (value.Source == BleValueChangeSource.Read)
                {
                    if (MainThread.IsMainThread)
                    {
                        ReadValues.Add(value);
                    }
                    else
                    {
                        await MainThread.InvokeOnMainThreadAsync(() =>
                        {
                            ReadValues.Add(value);
                        });
                    }
                }
            };
        }

        [RelayCommand]
        private async Task Write(IBleCharacteristic characteristic)
        {
            try
            {
                if (string.IsNullOrEmpty(WriteValue))
                {
                    await _dialogService.DisplayAlert("Error", "Please enter text to send.", "OK");
                    return;
                }

                var bytes = System.Text.Encoding.UTF8.GetBytes(WriteValue);
                await characteristic.WriteAsync(bytes);
                WriteValue = string.Empty; // Clear input after sending
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("Error", $"Failed to write: {ex.Message}", "OK");
            }
        }

        [RelayCommand]
        private async Task Read(IBleCharacteristic characteristic)
        {
            try
            {
                var value = await characteristic.ReadAsync();
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("Error", $"Failed to read: {ex.Message}", "OK");
            }
        }

        [RelayCommand]
        private async Task ToggleNotify(IBleCharacteristic characteristic)
        {
            try
            {
                if (characteristic.IsNotifying)
                {
                    await characteristic.StopNotificationsAsync();
                }
                else
                {
                    await characteristic.StartNotificationsAsync();
                }
            }
            catch (Exception ex)
            {
                await _dialogService.DisplayAlert("Error", $"Failed to toggle notification: {ex.Message}", "OK");
            }
        }

        #region bps

        private void CalculateBps(object sender, ElapsedEventArgs e)
        {
            MainThread.BeginInvokeOnMainThread(() =>
            {
                // calculate bps
                long writeDelta = _bytesTransferred[BleValueChangeSource.Write] - _lastBytesTransferred[BleValueChangeSource.Write];
                long readDelta = _bytesTransferred[BleValueChangeSource.Read] - _lastBytesTransferred[BleValueChangeSource.Read];
                long notifyDelta = _bytesTransferred[BleValueChangeSource.Notify] - _lastBytesTransferred[BleValueChangeSource.Notify];

                // convert to bps (1byte=8bit)
                WriteBps = writeDelta * 8; // bits per second
                ReadBps = readDelta * 8;
                NotifyBps = notifyDelta * 8;

                // updae max bps
                if (WriteBps > _maxBps[BleValueChangeSource.Write])
                {
                    _maxBps[BleValueChangeSource.Write] = WriteBps;
                    MaxWriteBps = WriteBps;
                }
                if (ReadBps > _maxBps[BleValueChangeSource.Read])
                {
                    _maxBps[BleValueChangeSource.Read] = ReadBps;
                    MaxReadBps = ReadBps;
                }
                if (NotifyBps > _maxBps[BleValueChangeSource.Notify])
                {
                    _maxBps[BleValueChangeSource.Notify] = NotifyBps;
                    MaxNotifyBps = NotifyBps;
                }

                // save the values for next calculation
                _lastBytesTransferred[BleValueChangeSource.Write] = _bytesTransferred[BleValueChangeSource.Write];
                _lastBytesTransferred[BleValueChangeSource.Read] = _bytesTransferred[BleValueChangeSource.Read];
                _lastBytesTransferred[BleValueChangeSource.Notify] = _bytesTransferred[BleValueChangeSource.Notify];
            });
        }

        // Reset
        public void ResetMaxBps()
        {
            _maxBps[BleValueChangeSource.Write] = 0;
            _maxBps[BleValueChangeSource.Read] = 0;
            _maxBps[BleValueChangeSource.Notify] = 0;
            MaxWriteBps = 0;
            MaxReadBps = 0;
            MaxNotifyBps = 0;
        }

        // Cleanup
        public void Dispose()
        {
            _bpsTimer?.Stop();
            _bpsTimer?.Dispose();
        }

        #endregion
    }
}
