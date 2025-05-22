using System.Globalization;

namespace MauiCrudApp.Ble.SwiftApp.Converters
{
    public class ByteArrayToHexConverter : IValueConverter
    {
        public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        {
            if (value is byte[] bytes)
            {
                // Convert to hex string (e.g., "F0 A1 3B")
                var hex = string.Join(" ", bytes.Select(b => b.ToString("X2")));
                // Truncate to avoid overly long strings (e.g., max 50 characters)
                return hex.Length > 50 ? hex.Substring(0, 50) + "..." : hex;
            }
            return string.Empty;
        }

        public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
