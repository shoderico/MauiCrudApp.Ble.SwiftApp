using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using MauiCrudApp.Ble.Interfaces;
using MauiCrudApp.Common.Interfaces;
using MauiCrudApp.Common.Navigation;
using MauiCrudApp.Common.ViewModels;

namespace MauiCrudApp.Ble.SwiftApp.Features.Characteristic.ViewModels
{
    public partial class CharacteristicControlViewModel : ViewModelBase<CharacteristicControlParameter>
    {
        private readonly INavigationService _navigationService;
        private readonly IDialogService _dialogService;
        private readonly IBleDeviceManager _bleDeviceManager;
        private readonly CharacteristicStateStore _characteristicStateStore;

        public CharacteristicControlViewModel(
                  INavigationParameterStore parameterStore
                , INavigationService navigationService
                , IDialogService dialogService
                , IBleDeviceManager bleDeviceManager
                , CharacteristicStateStore characteristicStateStore
            ) : base(
                parameterStore
            )
        {
            _navigationService = navigationService;
            _dialogService = dialogService;
            _bleDeviceManager = bleDeviceManager;
            _characteristicStateStore = characteristicStateStore;

            characteristicStateStore.Characteristics.CollectionChanged += OnCollectionChanged;
        }

        [ObservableProperty]
        private ObservableCollection<CharacteristicViewModel> characteristics = new();

        public override async Task InitializeAsync(CharacteristicControlParameter parameter, bool isInitialized)
        {
            await _characteristicStateStore.UpdateCharacteristicsAsync();
            Characteristics.Clear();
            foreach (var cvm in _characteristicStateStore.Characteristics)
            {
                Characteristics.Add(cvm);
            }
        }

        private void OnCollectionChanged(object? sender, System.Collections.Specialized.NotifyCollectionChangedEventArgs e)
        {
            var characteristics = _characteristicStateStore.Characteristics.ToList(); // copy
            Characteristics.Clear();
            foreach (var cvm in characteristics)
            {
                Characteristics.Add(cvm);
            }
        }

    }
}
