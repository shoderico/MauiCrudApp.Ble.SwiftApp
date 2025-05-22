using System.Globalization;

namespace MauiCrudApp.Ble.SwiftApp.Converters
{
    internal class ByteArrayToStringConverter : IValueConverter
    {
        public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        {
            if (value is byte[] bytes)
            {
                return System.Text.Encoding.UTF8.GetString(bytes);
            }
            return string.Empty;
        }

        public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
