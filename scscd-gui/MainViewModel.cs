using DynamicData;
using Mutagen.Bethesda;
using Mutagen.Bethesda.Environments;
using Mutagen.Bethesda.Fallout4;
using Mutagen.Bethesda.Installs;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Cache;
using Mutagen.Bethesda.Plugins.Cache.Internals.Implementations;
using Mutagen.Bethesda.Plugins.Order;
using Noggog;
using Reloaded.Memory.Utilities;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows.Data;
using System.Windows.Forms;
using System.Windows.Media; // for FolderBrowserDialog

namespace scscd_gui.wpf
{
    public class MainViewModel : INotifyPropertyChanged
    {
        public DirectoryPath dataDir;

        public event PropertyChangedEventHandler? PropertyChanged;
        private void OnPropertyChanged([CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        public ObservableCollection<ArmorListItem> ArmorItems { get; } = new();
        public ICollectionView ArmorView { get; }
        private IEnumerable<IAObjectModificationGetter>? allOmods;
        private ImmutableLoadOrderLinkCache<IFallout4Mod, IFallout4ModGetter>? linkCache;

        public ObservableCollection<OccCheck> Occupations { get; } = new();

        public ObservableCollection<ItemTypeVM> _clothingTypes = new();
        public ObservableCollection<ItemTypeVM> ClothingTypes
        {
            get => _clothingTypes;
        }

        /* Important: the order here represents its position in the bitmap. For example
         * Doctor is the rightmost bit in the map. */
        private readonly string[] _occupationNames =
            new[] { "Railroad (Runaway)",     // 10000000000000000000
                    "Railroad (Overt Agent)", // 01000000000000000000
                    "Minuteman",              // 00100000000000000000
                    "Institute (Soldier)",    // 00010000000000000000
                    "Institute (Assassin)",   // 00001000000000000000
                    "Gunner",                 // 00000100000000000000
                    "Raider",                 // 00000010000000000000
                    "BoS Soldier",            // 00000001000000000000
                    "BoS Support",            // 00000000100000000000
                    "Merchant",               // 00000000010000000000
                    "Citizen",                // 00000000001000000000
                    "Cultist",                // 00000000000100000000
                    "Drifter",                // 00000000000010000000
                    "Farmer",                 // 00000000000001000000
                    "Guard",                  // 00000000000000100000
                    "Scientist",              // 00000000000000010000
                    "Mercenary",              // 00000000000000001000
                    "Vault Dweller",          // 00000000000000000100
                    "Captive",                // 00000000000000000010
                    "Doctor"                  // 00000000000000000001
            };

        private readonly Dictionary<string, ArmorTags> _tagsByKey = new(StringComparer.OrdinalIgnoreCase);
        private HashSet<string> _selectedKeys = new(StringComparer.OrdinalIgnoreCase);

        public string _minLevel = "0";
        public string? MinLevel
        {
            get => _minLevel;
            set
            {
                Debug.WriteLine($"set MinLevel => {value}");
                _minLevel = value ?? "0";
                OnPropertyChanged();
                if (_suppress) return;
                if (value != "") ApplyMinLevelToSelection(int.Parse(_minLevel));
            }
        }
        private bool _suppress;

        private bool _isLoading;
        public bool IsLoading
        {
            get => _isLoading;
            set { if (_isLoading != value) { _isLoading = value; OnPropertyChanged(); } }
        }

        private string _loadingMessage = "Loading Fallout 4 data...";
        public string LoadingMessage
        {
            get => _loadingMessage;
            set { if (_loadingMessage != value) { _loadingMessage = value; OnPropertyChanged(); } }
        }

        private string? _csvPath;
        public string? CsvPath
        {
            get => _csvPath;
            set
            {
                if (_csvPath != value)
                {
                    _csvPath = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(HasCsv));
                }
            }
        }
        public bool HasCsv => !string.IsNullOrWhiteSpace(_csvPath);

        private string _editorFilter = string.Empty;
        public string EditorFilter
        {
            get => _editorFilter;
            set { if (_editorFilter != value) { _editorFilter = value; OnPropertyChanged(); ArmorView.Refresh(); } }
        }

        private string _pluginFilter = string.Empty;
        public string PluginFilter
        {
            get => _pluginFilter;
            set { if (_pluginFilter != value) { _pluginFilter = value; OnPropertyChanged(); ArmorView.Refresh(); } }
        }

        private bool _showUnderwears = true;
        public bool ShowUnderwears
        {
            get => _showUnderwears;
            set { if (_showUnderwears != value) { _showUnderwears = value; OnPropertyChanged(); ArmorView.Refresh(); } }
        }

        private bool _showOverwears = true;
        public bool ShowOverwears
        {
            get => _showOverwears;
            set { if (_showOverwears != value) { _showOverwears = value; OnPropertyChanged(); ArmorView.Refresh(); } }
        }

        private bool _hasSelection;
        public bool HasSelection
        {
            get => _hasSelection;
            private set { if (_hasSelection != value) { _hasSelection = value; OnPropertyChanged(); } }
        }

        // Tri-state (null = indeterminate)
        private bool? _maleState;
        public bool? MaleState
        {
            get => _maleState;
            set
            {
                if (_suppress) { _maleState = value; OnPropertyChanged(); return; }
                _maleState = value;
                OnPropertyChanged();
                if (value.HasValue) ApplyToSelection("male", value.Value);
            }
        }

        private bool? _femaleState;
        public bool? FemaleState
        {
            get => _femaleState;
            set
            {
                if (_suppress) { _femaleState = value; OnPropertyChanged(); return; }
                _femaleState = value;
                OnPropertyChanged();
                if (value.HasValue) ApplyToSelection("female", value.Value);
            }
        }

        private bool? _nsfwState;
        public bool? NSFWState
        {
            get => _nsfwState;
            set
            {
                if (_suppress) { _nsfwState = value; OnPropertyChanged(); return; }
                _nsfwState = value;
                OnPropertyChanged();
                if (value.HasValue) ApplyToSelection("nsfw", value.Value);
            }
        }

        private string? _clothingType;
        public string? ClothingType
        {
            get => _clothingType;
            set
            {
                if (_suppress) { _clothingType = value; OnPropertyChanged(); return; }
                _clothingType = value;
                OnPropertyChanged();
                ApplyClothingTypeToSelection(_clothingType);
            }
        }

        public MainViewModel()
        {
            // CollectionView for filtering
            ArmorView = CollectionViewSource.GetDefaultView(ArmorItems);
            ArmorView.Filter = FilterArmor;

            var view = CollectionViewSource.GetDefaultView(ClothingTypes);
            view.SortDescriptions.Clear();
            view.SortDescriptions.Add(new SortDescription(nameof(ItemTypeVM.Name), ListSortDirection.Ascending));

            // Build occupation items (tri-state checkboxes)
            foreach (var name in _occupationNames)
            {
                var oc = new OccCheck(name, onUserChanged: (_, newVal) =>
                {
                    if (_suppress) return;
                    if (newVal.HasValue) ApplyToSelection(name, newVal.Value);
                });
                Occupations.Add(oc);
            }
        }

        public async Task InitializeAsync()
        {
            IsLoading = true;
            LoadingMessage = "Loading Fallout 4 data...";
            try
            {
                // Pick Data folder on the UI thread (dialogs must be on UI thread)
                if (!new GameLocator().TryGetDataDirectory(GameRelease.Fallout4, out dataDir))
                    PromptForDataFolder();

                // Load clothing types before scanning armors, otherwise they won't auto detect.
                RefreshClothingTypes();

                // Do Mutagen scanning off the UI thread
                var armors = await Task.Run(() => ScanArmors(dataDir)); // returns List<ArmorListItem>

                // Apply results on the UI thread
                ArmorItems.Clear();
                foreach (var a in armors)
                {
                    ArmorItems.Add(a);
                    if (!_tagsByKey.ContainsKey(a.EditorID!))
                        _tagsByKey[a.EditorID!] = ArmorTags.Empty(_occupationNames);
                }

                RefreshAggregateStates();  // recompute checkboxes
            }
            finally
            {
                IsLoading = false;
            }
        }

        public void RefreshClothingTypes()
        {
            ClothingTypes.Clear();
            foreach (var t in new ItemTypeManagerVM(dataDir).ItemTypes)
                ClothingTypes.Add(t);
        }

        private static string PromptForDataFolder()
        {
            using var fbd = new FolderBrowserDialog
            {
                Description = "Locate your Fallout 4 Data\\ folder",
                UseDescriptionForTitle = true
            };
            if (fbd.ShowDialog() != System.Windows.Forms.DialogResult.OK)
                throw new Exception("No Data folder selected.");
            return fbd.SelectedPath;
        }

        public static BipedObjectFlag GetArmorBipedFlags(
                        IArmorGetter armo,
                        ILinkCache? cache = null,
                        bool includeArmaUnion = false)   // set true if you also want to union ARMA flags
        {
            // Prefer ARMO's own BOD2 if present
            var f = armo.BipedBodyTemplate?.FirstPersonFlags;
            if (f.HasValue) return f.Value;

            // Follow TemplateArmor if ARMO inherits its BOD2
            if (cache != null && !armo.TemplateArmor.IsNull &&
                armo.TemplateArmor.TryResolve(cache, out var templ))
            {
                var tf = templ.BipedBodyTemplate?.FirstPersonFlags;
                if (tf.HasValue) return tf.Value;
            }

            //// Optional: union flags from all linked ARMAs (some mods set them there)
            //if (includeArmaUnion && cache != null && armo.Armatures is not null)
            //{
            //    BipedObjectFlag acc = 0;
            //    foreach (var armaLink in armo.Armatures)
            //    {
            //        if (armaLink == null) continue;
            //        if (armaLink.TryResolve(cache, out IArmorAddonGetter arma))
            //        {
            //            var af = arma.BodyTemplate?.FirstPersonFlags;
            //            if (af.HasValue) acc |= af.Value;
            //        }
            //    }
            //    return acc;
            //}

            return 0;
        }

        // --- 2) Raw 64-bit mask (handy if you want bit math or numeric slot checks) ---

        public static ulong GetArmorBipedMask(
            IArmorGetter armo,
            ILinkCache? cache = null,
            bool includeArmaUnion = false)
        {
            return (ulong)GetArmorBipedFlags(armo, cache, includeArmaUnion);
        }

        // --- 3) Check a specific slot number (e.g., 33 = Body in FO4) ---

        public static bool HasBipedSlot(
            IArmorGetter armo,
            int slotIndex,                     // e.g., 33 for Body
            ILinkCache? cache = null,
            bool includeArmaUnion = false)
        {
            if (slotIndex < 0 || slotIndex >= 61) return false;
            ulong mask = GetArmorBipedMask(armo, cache, includeArmaUnion);
            ulong bit = 1UL << (slotIndex - 30);
            return (mask & bit) != 0;
        }

        private List<ArmorListItem> ScanArmors(string dataDir)
        {
            var env = GameEnvironment.Typical.Fallout4(Fallout4Release.Fallout4);

            // This mirrors the game: only enabled plugins, and only ones that actually exist on disk.
            IEnumerable<IModListingGetter<IFallout4ModGetter>> loadOrder = env.LoadOrder.WhereEnabledAndExisting().PriorityOrder;
            allOmods = loadOrder.AObjectModification().WinningOverrides();
            linkCache = loadOrder.ToImmutableLinkCache();

            var list = loadOrder.Armor().WinningOverrides()
                .Select(armo =>
                {
                    var eid = armo.EditorID ?? "(no EDID)";
                    var mod = armo.FormKey.ModKey.FileName;
                    var key = $"{eid}|{mod}"; // human-friendly + collision-resistant
                    return new ArmorListItem
                    {
                        EditorID = eid,
                        FullName = armo.Name?.String ?? "",
                        ModFile = mod,
                        FormKey = armo.FormKey.ToString(),
                        Armor = armo,
                        IsUnderwear = HasBipedSlot(armo, 36, linkCache)
                                    || HasBipedSlot(armo, 37, linkCache)
                                    || HasBipedSlot(armo, 38, linkCache)
                                    || HasBipedSlot(armo, 39, linkCache)
                                    || HasBipedSlot(armo, 40, linkCache),
                        SlotDescription = GetSlotDescription(eid, armo),
                        Key = key
                    };
                })
                .OrderBy(a => a.EditorID, StringComparer.OrdinalIgnoreCase)
                .ThenBy(a => a.ModFile, StringComparer.OrdinalIgnoreCase)
                .ToList();

            return list;
        }

        // ------------- Filtering -------------

        private bool FilterArmor(object obj)
        {
            if (obj is not ArmorListItem a) return false;

            if (!string.IsNullOrWhiteSpace(EditorFilter))
                if (a.EditorID?.IndexOf(EditorFilter, StringComparison.OrdinalIgnoreCase) < 0)
                    return false;

            if (!string.IsNullOrWhiteSpace(PluginFilter))
                if (a.ModFile?.IndexOf(PluginFilter, StringComparison.OrdinalIgnoreCase) < 0)
                    return false;

            if (a.IsUnderwear && !_showUnderwears)
                return false;

            if (!a.IsUnderwear && !_showOverwears)
                return false;

            return true;
        }

        // ------------- Selection & aggregate states -------------

        public void UpdateSelection(IList<ArmorListItem> selected)
        {
            _selectedKeys = new HashSet<string>(selected.Select(s => s.EditorID!), StringComparer.OrdinalIgnoreCase);
            HasSelection = _selectedKeys.Count > 0;
            RefreshAggregateStates();
        }

        public void RefreshAggregateStates()
        {
            _suppress = true;

            MaleState = Aggregate("male");
            FemaleState = Aggregate("female");
            NSFWState = Aggregate("nsfw");

            foreach (var oc in Occupations)
                oc.State = Aggregate(oc.Name);

            int ml = AggregateMinLevel();
            if (ml == -1) MinLevel = "";
            else MinLevel = ml.ToString();

            var itemTypes = new ItemTypeManagerVM(dataDir).ItemTypes;
            var typeNames = itemTypes.Select(x => x.Name).AsEnumerable();
            string currentType = AggregateClothingType();
            if (!currentType.Equals("<<multiple>>") && !typeNames.Contains(currentType))
                currentType = "DEFAULT";
            ClothingType = currentType;

            _suppress = false;
        }

        private bool? Aggregate(string field)
        {
            bool? first = null;
            foreach (var key in _selectedKeys)
            {
                var tags = GetOrCreateTags(key);
                var val = tags.Get(field);
                if (first is null) first = val;
                else if (first.Value != val) return null; // indeterminate
            }
            return first ?? false; // no selection -> false (UI disabled anyway)
        }

        private ArmorListItem? findArmorItem(string editorID)
        {
            foreach (ArmorListItem item in this.ArmorItems)
            {
                if (item.EditorID.Equals(editorID))
                    return item;
            }
            return null;
        }

        private string GetSlotDescription(string key, IArmorGetter? armor = null)
        {
            var tags = GetOrCreateTags(key);
            if (tags.clothingType != null) return tags.clothingType;

            var item = findArmorItem(key);
            if (armor == null) armor = item?.Armor;
            if (armor == null) return "None";
            HashSet<string> defSlots = new HashSet<string>();
            string? str = null;
            string? tag = null;
            // try to see if the used slots match a specific clothing type
            var selection = ClothingTypes.Where(x => {
                List<int> boundSlots = new List<int>();
                for (int i = 30; i < 64; i++)
                    if (HasBipedSlot(armor, i, linkCache))
                        boundSlots.Add(i);
                List<int> xSlots = x.Slots.Where(y => y.IsArmoChecked).Select(y => y.SlotIndex).ToList();
                xSlots.Sort();
                boundSlots.Sort();
                if (xSlots.SequenceEqual(boundSlots))
                    // the biped slots on this armor exactly match the equipped slots on this clothing type.
                    return true;
                else
                    // a slot is biped on the armor but doesn't match any slots
                    // for this clothing type
                    return false;
            });
            if (selection.Any())
            {
                // slots match at least one existing clothing type even though
                // the item wasn't explicitly assigned to one
                foreach (var sel in selection)
                    defSlots.Add(sel.Name);
                tag = "AUTO";
            }
            else
            {
                // slots did not match any existing clothing type, so
                // just list the slots themselves
                for (int i = 30; i < 64; i++)
                    if (HasBipedSlot(armor, i, linkCache))
                        if (SlotCatalog.ById.ContainsKey(i))
                            defSlots.Add(SlotCatalog.ById[i].name);
                        else
                            defSlots.Add("" + i);
                tag = "SLOT";
            }
            foreach (var n in defSlots)
                str = (str == null ? "" + n : str + ", " + n);
            str = tag + ": " + str;
            return str;
        }

        // Clothing type is probably the most cumbersome part to hook up and important to get
        // right, due to clipping. So we want this to be as helpful as possible, but have limited
        // space to work with.
        // If an item is selected, we want to try to indicate first if it has an explicit clothing
        // type. It of course won't if the mod has no CSV, which is when it's hardest to work with.
        // So, if it has no clothing type, we'll next look at its biped slots. If they exactly match
        // an existing clothing type, we'll indicate that, so that the user knows it is compatible
        // with the existing mapping (or it will have the wrong name, e.g. "Hat" when it should
        // be "Gloves"). If there is no matching clothing item at all, we'll just show the slot names
        // as defined in ClothingTypeModel.
        // If there are multiple selected, we'll try to follow the same rule, and use "<<multiple>>"
        // if there are several distinct layouts in use, to indicate that they maybe shouldn't be
        // grouped together.
        private string AggregateClothingType()
        {
            string? aggregate = null;
            foreach (var key in _selectedKeys)
            {
                var tags = GetOrCreateTags(key);
                var ct = tags.clothingType;
                if (ct == null)
                {
                    // item has no clothing type assigned, get its used slots
                    string str = GetSlotDescription(key);
                    if (aggregate == null)
                        aggregate = str;
                    else
                    {
                        // another item already populated `aggregate`; if the aggregates
                        // match there is no issue, otherwise return indicating they
                        // follow differen layouts.
                        if (!aggregate.Equals(str))
                            return "<<multiple>>";
                    }
                }
                else
                {
                    if (aggregate == null) aggregate = ct;
                    if (aggregate != null && !aggregate.Equals(ct))
                        return "<<multiple>>";
                }
            }
            return aggregate == null ? "None" : aggregate;
        }

        private int AggregateMinLevel()
        {
            int ml = -1;
            foreach (var key in _selectedKeys) {
                var tags = GetOrCreateTags(key);
                var minLevel = tags.minLevel;
                if (ml == -1) ml = minLevel;
                if (minLevel != ml)
                {
                    // found a difference; selected armors target different min levels!
                    return -1;
                }
            }
            // all items matched, or none are selected.
            if (ml == -1) return 0; // ml default is 0
            return ml;
        }

        private void ApplyToSelection(string field, bool value)
        {
            _suppress = true;
            foreach (var key in _selectedKeys)
                GetOrCreateTags(key).Set(field, value);
            _suppress = false;

            // Recompute aggregates so UI moves out of "mixed"
            RefreshAggregateStates();
        }

        private void ApplyClothingTypeToSelection(string? clothingType)
        {
            _suppress = true;
            if (!ClothingTypes.Select(x => x.Name).Where(x => x.Equals(clothingType)).Any())
                // clothingType is not valid, we assume it's meant to represent default/no type override
                clothingType = null;
            foreach (var key in _selectedKeys)
            {
                GetOrCreateTags(key).clothingType = clothingType;
                var a = findArmorItem(key);
                if (a != null) a.SlotDescription = GetSlotDescription(key);
            }
            _suppress = false;
            RefreshAggregateStates();
        }

        private void ApplyMinLevelToSelection(int minLevel)
        {
            _suppress = true;
            Debug.WriteLine($"apply min level {minLevel} to {_selectedKeys.Count()} keys");
            foreach (var key in _selectedKeys)
            {
                GetOrCreateTags(key).minLevel = minLevel;
            }
            _suppress = false;

            // Recompute aggregates so UI moves out of "mixed"
            RefreshAggregateStates();
        }

        private ArmorTags GetOrCreateTags(string key)
        {
            if (!_tagsByKey.TryGetValue(key, out var tags))
            {
                tags = ArmorTags.Empty(_occupationNames);
                _tagsByKey[key] = tags;
            }
            return tags;
        }

        // ------------- CSV I/O -------------

        public void LoadCsvFrom(string path)
        {
            if (!File.Exists(path))
            {
                Debug.WriteLine($"File {path} does not exist");
                return;
            }
            UnloadCsvAndClear();
            string onlyFilename = Path.GetFileName(path);  // "example.csv"
            string pluginName = onlyFilename;
            if (onlyFilename.EndsWith(".csv", StringComparison.OrdinalIgnoreCase))
            {
                pluginName = pluginName.Substring(0, pluginName.Length - 4);
                Debug.WriteLine($"Plugin name is {pluginName}");
            }

            var lines = File.ReadAllLines(path);
            Debug.WriteLine($"CSV contains {lines.Length} lines");
            if (lines.Length == 0)
                return;

            var header = SplitCsvLine(lines[0]);
            //var idx = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            //for (int i = 0; i < header.Count; i++) idx[header[i]] = i;

            for (int i = 1; i < lines.Length; i++)
            {
                // strip comments
                if (lines[i].IndexOf("#") >= 0)
                    lines[i] = lines[i].Substring(0, lines[i].IndexOf("#"));
                // skip blank lines
                if (lines[i].Trim().Length == 0)
                {
                    Debug.WriteLine($"line {i+1} is blank");
                    continue;
                }
                var parts = SplitCsvLine(lines[i]);
                Debug.WriteLine($"num parts={parts.Count}");
                if (parts.Count == 0) continue;
                var tags = ArmorTags.Empty(_occupationNames);

                // Sex
                var sexBits = parts[0].Trim().ToCharArray();
                if (sexBits[0] == '1') tags.Set("female", true);
                if (sexBits[1] == '1') tags.Set("male", true);

                // OccupationBits
                var occupationBitsStr = parts[1].Trim();
                // set leftmost bits to zero in case we added new occupations and this
                // is an older file (meaning shorter bitstring). The contract is
                // we always add bits to the left so a shorter string just implies
                // unset bits on the left.
                while (occupationBitsStr.Length < _occupationNames.Length)
                    occupationBitsStr = "0" + occupationBitsStr;
                var occupationBits = occupationBitsStr.ToCharArray();
                for (int occIdx = 0; occIdx < _occupationNames.Length; occIdx++)
                    tags.Set(_occupationNames[occIdx], occupationBits[occIdx] == '1');

                // EditorID (formerly FormID)
                string editorID = parts[2].Trim();
                _tagsByKey[editorID] = tags;

                // Old format: if it looks like hex, it's a form ID. We can use the CSV filename
                // to convert it into a form key, and use the form key to look up the editor ID.
                Debug.WriteLine($"seen editorID={editorID}");
                if (editorID.Length == 8 && editorID.All(c => Uri.IsHexDigit(c)))
                {
                    string localizedFormID = editorID.Substring(2, 6);
                    string formKey = localizedFormID + ":" + pluginName;
                    // try to detect light forms for ESL-flagged ESPs.
                    string lightFormKey = localizedFormID.Substring(3, 3) + ":" + pluginName;
                    bool found = false;
                    foreach (var item in ArmorItems)
                    {
                        var armorKey = item.Armor!.FormKey.ToString();
                        var armorKeyLt = armorKey.Substring(3, armorKey.Length - 3);
                        bool isLight = armorKey.Substring(0, 3) == "000"; // heuristic, light form IDs can't exceed 000FFF
                        if (!isLight && formKey.Equals(armorKey))
                            found = true;
                        else if (isLight && lightFormKey.Equals(armorKeyLt))
                            found = true;
                        if (found)
                        {
                            found = true;
                            editorID = item.Armor!.EditorID!;
                            Debug.WriteLine($"Converted form ID {localizedFormID} to editor ID {editorID}");
                            break;
                        }
                    }
                    if (!found)
                    {
                        // this is intentional behavior for some files
                        Debug.WriteLine($"[WARN] Failed to find form using form key '{formKey}'");
                    }
                }

                // MinLevel
                if (parts.Count() >= 4)
                    tags.minLevel = parts[3].Trim().Length > 0 ? int.Parse(parts[3].Trim()) : 0;

                // OMods
                // Ignored, they get regenerated on export

                if (parts.Count() >= 6)
                    tags.Set("nsfw", parts[5].Trim() == "1" || parts[5].Trim() == "true");

                if (parts.Count() >= 7 && parts[6].Trim().Length > 0)
                {
                    tags.clothingType = parts[6].Trim();
                    var armor = findArmorItem(editorID);
                    if (armor != null)
                    {
                        armor.SlotDescription = GetSlotDescription(editorID, armor.Armor);
                    }
                }
            }
        }

        private string OccupationBitstr(ArmorTags? tags)
        {
            var str = "";
            if (tags != null)
                foreach (var occ in _occupationNames)
                    str += (tags.Get(occ) ? "1" : "0");
            return str;
        }

        public void SaveCsvTo(string path)
        {
            var header = new List<string> { "Sex", "Occupation", "EditorID", "MinLevel", "Compatible OMODs", "Is NSFW?", "Clothing Type" };

            using var sw = new StreamWriter(path, false);
            sw.WriteLine(string.Join(",", header));

            foreach (var item in ArmorItems)
            {
                _tagsByKey.TryGetValue(item.EditorID!, out var tags);
                tags ??= ArmorTags.Empty(_occupationNames);

                string occupBits = OccupationBitstr(tags);
                // If no bits are set, we don't emit this row as it can't be used.
                if (occupBits.IndexOf("1") >= 0)
                {
                    String omodsList = "";
                    var compatibleOmods = Omods.FindCompatibleOmodsForArmor(item.Armor!, allOmods!, linkCache);
                    foreach (var omod in compatibleOmods)
                    {
                        if (!omodsList.Equals("")) omodsList += ";";
                        omodsList += omod.EditorID;
                    }

                    var row = new List<string>
                    {
                        tags.Get("male") && tags.Get("female") ? "11" : tags.Get("female") ? "10" : "01",
                        Csv(occupBits),
                        Csv(item.EditorID ?? ""),
                        tags.minLevel.ToString(),
                        omodsList,
                        tags.Get("nsfw") ? "1" : "0",
                        tags.clothingType?.Contains(',') == true ? $"\"{tags.clothingType}\"" : tags.clothingType ?? ""
                    };

                    sw.WriteLine(string.Join(",", row));
                }
            }
        }

        public void UnloadCsvAndClear()
        {
            // Forget the current file
            CsvPath = null;

            // Reset all tags to defaults (unchecked) for every listed armor
            foreach (var item in ArmorItems)
                _tagsByKey[item.EditorID!] = ArmorTags.Empty(_occupationNames);

            // Recompute aggregate tri-states for current selection
            RefreshAggregateStates();
        }

        // --- CSV helpers ---

        private static string Csv(string s) =>
            s.Contains(',') || s.Contains('"') ? $"\"{s.Replace("\"", "\"\"")}\"" : s;

        private static List<string> SplitCsvLine(string line)
        {
            var result = new List<string>();
            if (string.IsNullOrEmpty(line)) return result;

            bool inQuotes = false;
            var cur = new System.Text.StringBuilder();
            for (int i = 0; i < line.Length; i++)
            {
                char c = line[i];
                if (inQuotes)
                {
                    if (c == '"')
                    {
                        if (i + 1 < line.Length && line[i + 1] == '"') { cur.Append('"'); i++; }
                        else inQuotes = false;
                    }
                    else cur.Append(c);
                }
                else
                {
                    if (c == ',') { result.Add(cur.ToString()); cur.Clear(); }
                    else if (c == '"') inQuotes = true;
                    else cur.Append(c);
                }
            }
            result.Add(cur.ToString());
            return result;
        }

        private static string GetSafe(List<string> parts, Dictionary<string, int> idx, string name) =>
            idx.TryGetValue(name, out var i) && i < parts.Count ? parts[i] : "";

        private static bool ReadBool(List<string> parts, Dictionary<string, int> idx, string name) =>
            idx.TryGetValue(name, out var i) && i < parts.Count &&
            (parts[i] == "1" || parts[i].Equals("true", StringComparison.OrdinalIgnoreCase));
    }
}
