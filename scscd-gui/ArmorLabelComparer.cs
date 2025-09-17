using System.Collections;
using System.Globalization;
using System.Windows.Data;

namespace scscd_gui.wpf
{
    public sealed class LabelComparer : IComparer
    {
        private readonly IValueConverter _converter;
        public LabelComparer(IValueConverter converter) => _converter = converter;

        public int Compare(object? x, object? y)
        {
            var lx = (string?)_converter.Convert(x!, typeof(string), null, CultureInfo.CurrentCulture) ?? "";
            var ly = (string?)_converter.Convert(y!, typeof(string), null, CultureInfo.CurrentCulture) ?? "";
            return string.Compare(lx, ly, CultureInfo.CurrentCulture, CompareOptions.IgnoreCase);
        }
    }
}
