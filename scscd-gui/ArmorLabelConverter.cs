using System.Globalization;
using System.Windows.Data;

namespace scscd_gui.wpf
{
    public sealed class ArmorLabelConverter : IValueConverter
    {
        public object? Convert(object value, Type? t, object? p, CultureInfo culture)
        {
            var a = value as ArmorListItem;
            if (a is null) return string.Empty;
            var eid = a.EditorID ?? "(no EDID)";
            var full = a.FullName ?? "";
            var fk = a.FormKey ?? "";
            return $"{eid} - {full}";
        }

        public object? ConvertBack(object v, Type? t, object? p, CultureInfo c) => throw new NotSupportedException();
    }
}
