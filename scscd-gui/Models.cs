using Mutagen.Bethesda.Fallout4;
using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace scscd_gui.wpf
{
    public class ArmorListItem : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        public string Key { get; set; } = "";
        public string? EditorID { get; set; }
        public string? FullName { get; set; }
        public string? ModFile { get; set; }
        public string? FormKey { get; set; }
        public IArmorGetter? Armor { get; set; }
        public bool IsUnderwear { get; set; }
        public bool IsLight { get; set; } // is the containing plugin ESL flagged?
        
        // Human-readable description of the slots this armor uses or its autodetected clothing type
        private string? _slotDescription;
        public string? SlotDescription
        {
            get => _slotDescription;
            set
            {
                if (_slotDescription != value)
                {
                    _slotDescription = value;
                    OnPropertyChanged();
                }
            }
        }

    }

    public class ArmorTags
    {
        private readonly System.Collections.Generic.HashSet<string> _fields;
        private readonly System.Collections.Generic.Dictionary<string, bool> _values;
        public int minLevel;
        public string? clothingType;

        private ArmorTags(System.Collections.Generic.HashSet<string> fields,
                          System.Collections.Generic.Dictionary<string, bool> values)
        {
            _fields = fields;
            _values = values;
            minLevel = 0;
            clothingType = null;
        }

        public static ArmorTags Empty(string[] occupations)
        {
            var fields = new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase)
            { "male", "female", "nsfw" };
            foreach (var o in occupations) fields.Add(o);

            var dict = new System.Collections.Generic.Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            foreach (var f in fields) dict[f] = false;

            return new ArmorTags(fields, dict);
        }

        public bool Get(string field) => _values.TryGetValue(field, out var v) && v;
        public void Set(string field, bool value)
        {
            if (_fields.Contains(field)) _values[field] = value;
        }
    }

    public class OccCheck : INotifyPropertyChanged
    {
        private bool? _state; // null = indeterminate
        private readonly Action<string, bool?>? _onUserChanged;

        public event PropertyChangedEventHandler? PropertyChanged;
        private void OnPropertyChanged([CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        public string Name { get; }

        public bool? State
        {
            get => _state;
            set
            {
                if (_state == value) return;
                _state = value;
                OnPropertyChanged();
                _onUserChanged?.Invoke(Name, value);
            }
        }

        public OccCheck(string name, Action<string, bool?>? onUserChanged = null)
        {
            Name = name;
            _onUserChanged = onUserChanged;
        }
    }
}
