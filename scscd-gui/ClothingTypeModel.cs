using DynamicData;
using DynamicData.Binding;
using Mutagen.Bethesda.Skyrim;
using Noggog;
using OneOf.Types;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Shapes;

public class SlotVM : INotifyPropertyChanged
{
    int _slotIndex;
    string _name = "";
    bool _isArmoChecked;
    bool _isArmaChecked;

    public int SlotIndex
    {
        get => _slotIndex;
        set { _slotIndex = value; OnPropertyChanged(); }
    }

    public string Name
    {
        get => _name;
        set { _name = value; OnPropertyChanged(); }
    }

    public bool IsArmoChecked
    {
        get => _isArmoChecked;
        set { _isArmoChecked = value; OnPropertyChanged(); }
    }

    public bool IsArmaChecked
    {
        get => _isArmaChecked;
        set { _isArmaChecked = value; OnPropertyChanged(); }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    void OnPropertyChanged([CallerMemberName] string? name = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}

public class ItemTypeVM : INotifyPropertyChanged
{
    readonly ItemTypeManagerVM _owner;     // used for uniqueness checks
    string _name = "";
    string _sourceCsv = "";
    public string Name
    {
        get => _name;
        set
        {
            _name = value;
            if (string.IsNullOrWhiteSpace(value))
                _name = "Unnamed";
            else
            {
                if (_owner.ItemTypes.Any(it => !ReferenceEquals(it, this) &&
                                         string.Equals(it.Name, Name, System.StringComparison.OrdinalIgnoreCase)))
                {
                    _name = _owner.MakeUnique(value);
                }
                else
                {
                    _name = value;
                }
                OnPropertyChanged();
            }
        }
    }
    public string SourceCsv {
        get => _sourceCsv;
        set {
            if (!string.IsNullOrWhiteSpace(value))
            {
                _sourceCsv = value;
                _owner.LastUsedCsv = _sourceCsv;
                _owner.RefreshCsvChoices();
                OnPropertyChanged();
            }
        }
    }
    public ItemTypeVM(ItemTypeManagerVM owner) => _owner = owner;


    public ObservableCollection<SlotVM> Slots { get; } = new();

    public event PropertyChangedEventHandler? PropertyChanged;
    void OnPropertyChanged([CallerMemberName] string? n = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
}
public readonly record struct Slot(int slotIndex, string name);
public static class SlotCatalog {
    public static Slot[] All = {
        new(/* BODY */      33, "Boots (vanilla BODY)"),
        new(/* Scalp     */ 52, "Scalp"),
        new(/* Hair Top  */ 30, "Hair (Top)"),
        new(/* Hair Long */ 31, "Hair (Long)"),
        new(/* Headband  */ 46, "Facegear"),
        new(/* Eyes      */ 47, "Eyes"),
        new(/* Neck      */ 50, "Neck"),
        new(/* [U] Torso */ 36, "Chest Underwear"),
        new(/* [A] Torso */ 41, "Chest Outerwear"),
        new(/* Unnamed   */ 56, "Abdomen Underwear"),
        new(/* Unnamed   */ 57, "Abdomen Outerwear"),
        new(/* Unnamed   */ 55, "Pelvis Underwear"),
        new(/* Unnamed   */ 54, "Pelvis Outerwear"),
        new(/* [U] L Arm */ 37, "L Arm & Shoulder Underwear"),
        new(/* [A] L Arm */ 42, "L Arm & Shoulder Outerwear"),
        new(/* [U] R Arm */ 38, "R Arm & Shoulder Underwear"),
        new(/* [A] R Arm */ 43, "R Arm & Shoulder Outerwear"),
        new(/* Ring      */ 51, "Cloak/Cape/Poncho"),
        new(/* L Hand    */ 34, "L Hand"),
        new(/* R Hand    */ 35, "R Hand"),
        new(/* [U] L Leg */ 39, "L Leg Underwear"),
        new(/* [A] L Leg */ 44, "L Leg Outerwear"),
        new(/* [U] R Leg */ 40, "R Leg Underwear"),
        new(/* [A] R Leg */ 45, "R Leg Outerwear"),
        new(/* Beard     */ 48, "Backpack/Rig"),
        new(/* Mouth     */ 49, "Hip/Drop-Leg Rig"),
        new(/* Unnamed   */ 58, "Body Jewelry"),
    };

    public static IReadOnlyDictionary<int, Slot> ById = All.ToDictionary(s => s.slotIndex);
}

public static class SlotFactory
{
    public static ObservableCollection<SlotVM> CreateBlankSet()
    {
        var list = new ObservableCollection<SlotVM>();
        foreach (var def in SlotCatalog.All)
            list.Add(new SlotVM { SlotIndex = def.slotIndex, Name = def.name });
        return list;
    }
}

public class ItemTypeManagerVM : INotifyPropertyChanged
{
    private DirectoryPath dataDir;

    public ObservableCollection<ItemTypeVM> ItemTypes { get; } = new();
    public ObservableCollection<string> CsvChoices { get; } = new();

    ItemTypeVM? _selectedItemType;
    public ItemTypeVM? SelectedItemType
    {
        get => _selectedItemType;
        set { _selectedItemType = value; OnPropertyChanged(); }
    }

    public RelayCommand AddItemTypeCommand { get; }
    public RelayCommand RemoveItemTypeCommand { get; }
    public RelayCommand SaveAllCommand { get; }
    public ICommand BrowseCsvCommand { get; }

    static string _lastUsedCsv = "user\\custom.csv";
    public string LastUsedCsv
    {
        get => _lastUsedCsv;
        set { if (!string.IsNullOrWhiteSpace(value)) _lastUsedCsv = value; }
    }

    public ItemTypeManagerVM(DirectoryPath dataDir)
    {
        var view = CollectionViewSource.GetDefaultView(ItemTypes);
        view.SortDescriptions.Clear();
        view.SortDescriptions.Add(new SortDescription(nameof(ItemTypeVM.Name), ListSortDirection.Ascending));

        this.dataDir = System.IO.Path.Combine(dataDir, "F4SE\\Plugins\\scscd\\taxonomy");
        LoadAllCsvsRecursive();
        RefreshCsvChoices();

        BrowseCsvCommand = new RelayCommand(p => BrowseCsv(p as ItemTypeVM));
        AddItemTypeCommand = new RelayCommand(_ => AddItemType());
        RemoveItemTypeCommand = new RelayCommand(_ => RemoveSelected(), _ => SelectedItemType != null);
        SaveAllCommand = new RelayCommand(_ => SaveAll());
    }

    void LoadAllCsvsRecursive()
    {
        foreach (var abs in Directory.EnumerateFiles(dataDir, "*.csv", SearchOption.AllDirectories))
        {
            var rel = ToRelative(abs);
            foreach (var item in ParseCsv(abs))
            {
                // enforce global unique Name
                if (ItemTypes.Any(it => it.Name.Equals(item.Name, StringComparison.OrdinalIgnoreCase)))
                    item.Name = MakeUnique(item.Name);
                item.SourceCsv = rel;
                ItemTypes.Add(item);
            }
        }
    }

    // Stub: parse your CSV and yield ItemTypeVMs (fill Slots etc.)
    IEnumerable<ItemTypeVM> ParseCsv(string absolutePath)
    {
        var rel = ToRelative(absolutePath);
        bool haveHeader = false;
        foreach (var line in File.ReadLines(absolutePath))
        {
            // skip header (first line)
            if (!haveHeader)
            {
                haveHeader = true;
                continue;
            }
            if (string.IsNullOrWhiteSpace(line)) continue;
            if (line.TrimStart().StartsWith("#")) continue; // allow comments

            // split into at most 3 columns: name, armoList, armaList
            var parts = SplitCsvLine(line, 3);
            if (parts.Count == 0) continue;

            var name = parts[0].Trim();
            var armoIds = ParseIdList(parts.Count > 1 ? parts[1] : "");
            var armaIds = ParseIdList(parts.Count > 2 ? parts[2] : "");

            var it = new ItemTypeVM(this) { Name = name, SourceCsv = rel, };
            it.Slots.Clear();
            var slots = SlotFactory.CreateBlankSet();
            // mark checks
            foreach (var s in slots)
            {
                var id = s.SlotIndex;
                s.IsArmoChecked = armoIds.Contains(id);
                s.IsArmaChecked = armaIds.Contains(id);
                it.Slots.Add(s);
            }
            yield return it;
        }
    }

    static HashSet<int> ParseIdList(string text)
    {
        var set = new HashSet<int>();
        if (string.IsNullOrWhiteSpace(text)) return set;
        foreach (var tok in text.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries))
        {
            if (int.TryParse(tok.Trim(), out var id)) set.Add(id);
        }
        return set;
    }

    // very small CSV helper: split by commas, allowing commas in quotes
    static List<string> SplitCsvLine(string line, int maxColumns)
    {
        var result = new List<string>();
        bool inQuotes = false;
        var sb = new System.Text.StringBuilder();
        foreach (var ch in line)
        {
            if (ch == '"') { inQuotes = !inQuotes; continue; }
            if (ch == ',' && !inQuotes)
            {
                result.Add(sb.ToString());
                sb.Clear();
                if (result.Count == maxColumns - 1) // last column: take the rest as-is
                {
                    sb.Append(line.AsSpan(line.IndexOf(',') + 1));
                    break;
                }
                continue;
            }
            sb.Append(ch);
        }
        if (sb.Length > 0 || line.EndsWith(",")) result.Add(sb.ToString());
        return result;
    }

    private string ToRelative(string fullPath)
    {
        try
        {
            var root = System.IO.Path.GetFullPath(dataDir)
                                      .TrimEnd(System.IO.Path.DirectorySeparatorChar, System.IO.Path.AltDirectorySeparatorChar)
                                      + System.IO.Path.DirectorySeparatorChar;

            var full = System.IO.Path.GetFullPath(fullPath);
            if (full.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                return full.Substring(root.Length);
        }
        catch { /* ignore, fall back to filename */ }

        // fallback: just return filename
        return System.IO.Path.GetFileName(fullPath);
    }

    private string ToAbsolute(string relativePath)
    {
        // Combine with CSV root
        return System.IO.Path.GetFullPath(
            System.IO.Path.Combine(dataDir, relativePath ?? ""));
    }

    public void RefreshCsvChoices()
    {
        var discovered = Directory.Exists(dataDir)
            ? Directory.EnumerateFiles(dataDir, "*.csv", SearchOption.AllDirectories)
                      .Select(ToRelative)
            : Enumerable.Empty<string>();

        var fromRows = ItemTypes.Select(i => i.SourceCsv)
                                .Where(s => !string.IsNullOrWhiteSpace(s));

        var unique = discovered.Concat(fromRows)
                               .Distinct(StringComparer.OrdinalIgnoreCase)
                               .OrderBy(s => s, StringComparer.OrdinalIgnoreCase)
                               .ToList();

        CsvChoices.Clear();
        foreach (var s in unique) CsvChoices.Add(s);
    }

    void BrowseCsv(ItemTypeVM? row)
    {
        if (row == null) return;

        var dlg = new SaveFileDialog
        {
            Title = "Choose or create a CSV",
            Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
            AddExtension = true,
            DefaultExt = "csv",
            InitialDirectory = dataDir,
            RestoreDirectory = true,
            OverwritePrompt = false,
            CreatePrompt = false,
            FileName = string.IsNullOrWhiteSpace(row.SourceCsv)
                ? "new.csv"
                : row.SourceCsv
        };

        if (dlg.ShowDialog() == DialogResult.OK)
        {
            var full = System.IO.Path.IsPathRooted(dlg.FileName)
                 ? dlg.FileName
                 : System.IO.Path.GetFullPath(System.IO.Path.Combine(dlg.InitialDirectory ?? dataDir, dlg.FileName));
            row.SourceCsv = ToRelative(full);
            LastUsedCsv = row.SourceCsv;
            RefreshCsvChoices();
        }
    }

    void AddItemType()
    {
        var it = new ItemTypeVM(this) { Name = MakeUnique("New Item"), SourceCsv = LastUsedCsv };
        foreach (Slot slot in SlotCatalog.All)
        {
            it.Slots.Add(new SlotVM { SlotIndex = slot.slotIndex, Name = slot.name, IsArmoChecked = false, IsArmaChecked = false });
        }
        ItemTypes.Add(it);
        SelectedItemType = it;
    }

    void SaveAll()
    {
        // group item types by their target CSV (relative)
        var groups = ItemTypes.GroupBy(it => it.SourceCsv ?? "", StringComparer.OrdinalIgnoreCase);

        foreach (var g in groups)
        {
            var abs = ToAbsolute(g.Key);
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(abs)!);

            var header = new List<string> { "Name","ARMO Slots","ARMA Slots" };
            using var sw = new StreamWriter(abs, false);
            sw.WriteLine(string.Join(",", header));

            foreach (var it in g)
            {
                var armo = it.Slots
                             .Select(s => (SlotIndex: s.SlotIndex, Name: s.Name, Checked: s.IsArmoChecked))
                             .Where(x => x.Checked)
                             .Select(x => x.SlotIndex)
                             .OrderBy(x => x);

                var arma = it.Slots
                             .Select(s => (SlotIndex: s.SlotIndex, Name: s.Name, Checked: s.IsArmaChecked))
                             .Where(x => x.Checked)
                             .Select(x => x.SlotIndex)
                             .OrderBy(x => x);

                string col1 = string.Join(';', armo);
                string col2 = string.Join(';', arma);

                // simple CSV: quote the name if it contains commas
                string name = it.Name?.Contains(',') == true ? $"\"{it.Name}\"" : it.Name ?? "";
                sw.WriteLine($"{name},{col1},{col2}");
            }
        }

        RefreshCsvChoices(); // in case new files appeared
    }

    void RemoveSelected()
    {
        if (SelectedItemType is null) return;
        var idx = ItemTypes.IndexOf(SelectedItemType);
        ItemTypes.Remove(SelectedItemType);
        SelectedItemType = ItemTypes.Count == 0 ? null : ItemTypes[Math.Clamp(idx - 1, 0, ItemTypes.Count - 1)];
        RefreshCsvChoices();
    }

    public string MakeUnique(string baseName)
    {
        var name = baseName;
        int i = 1;
        while (ItemTypes.Any(x => string.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase)))
            name = $"{baseName} {++i}";
        return name;
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    void OnPropertyChanged([CallerMemberName] string? name = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}

// Bare-bones RelayCommand
public sealed class RelayCommand : System.Windows.Input.ICommand
{
    readonly System.Action<object?> _execute;
    readonly System.Predicate<object?>? _canExecute;
    public RelayCommand(System.Action<object?> execute, System.Predicate<object?>? canExecute = null)
    { _execute = execute; _canExecute = canExecute; }
    public bool CanExecute(object? parameter) => _canExecute?.Invoke(parameter) ?? true;
    public void Execute(object? parameter) => _execute(parameter);
    public event System.EventHandler? CanExecuteChanged
    { add { CommandManager.RequerySuggested += value; } remove { CommandManager.RequerySuggested -= value; } }
}
