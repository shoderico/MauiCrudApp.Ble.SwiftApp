namespace MauiCrudApp.Ble.SwiftApp.Converters
{
    internal class BoolToStringConverter : IValueConverter
    {
        public object Convert(object? value, Type targetType, object? parameter, System.Globalization.CultureInfo culture)
        {

            return (bool)(value ?? false) ? "New" : "Edit";
        }

        public object ConvertBack(object? value, Type targetType, object? parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
