using Microsoft.Win32;
using System;
using System.ComponentModel;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;

namespace scscd_gui.wpf
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _vm = new();
        private GridViewColumnHeader? _lastHeaderClicked;
        private ListSortDirection _lastDirection = ListSortDirection.Ascending;

        public MainWindow()
        {
            InitializeComponent();
            DataContext = _vm;
        }

        private void GridViewColumnHeader_Click(object sender, RoutedEventArgs e)
        {
            if (e.OriginalSource is not GridViewColumnHeader header || header.Column == null)
                return;

            string? sortBy = header.Tag as string;
            if (string.IsNullOrEmpty(sortBy))
                return;

            ListSortDirection direction = ListSortDirection.Ascending;
            if (_lastHeaderClicked == header && _lastDirection == ListSortDirection.Ascending)
                direction = ListSortDirection.Descending;

            var view = CollectionViewSource.GetDefaultView(ArmorList.ItemsSource);
            view.SortDescriptions.Clear();
            view.SortDescriptions.Add(new SortDescription(sortBy, direction));
            view.Refresh();

            // remember for next click
            _lastHeaderClicked = header;
            _lastDirection = direction;
        }

        private async void ManageClothingTypes_Click(object sender, RoutedEventArgs e)
        {
            var window = new ClothingTypesWindow(_vm.dataDir)
            {
                Owner = this, // sets window ownership for centering/stacking
                WindowStartupLocation = WindowStartupLocation.CenterOwner
            };
            window.Closed += (_, __) => _vm.RefreshClothingTypes();
            window.Show(); // or: _info.ShowDialog();
        }

        private async void Window_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                await _vm.InitializeAsync();
                var cvs = (CollectionViewSource)FindResource("ArmorCvs");
                if (cvs.View is ListCollectionView lcv)
                {
                    var conv = (IValueConverter)FindResource("ArmorLabelConverter");
                    lcv.CustomSort = new LabelComparer(conv);
                }
            }
            catch (Exception ex)
            {
                System.Windows.MessageBox.Show(this, ex.ToString(), "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ArmorList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var selected = ArmorList.SelectedItems.Cast<ArmorListItem>().ToList();
            _vm.UpdateSelection(selected);
        }

        private void ClearFilters_Click(object sender, RoutedEventArgs e)
        {
            _vm.EditorFilter = string.Empty;
            _vm.PluginFilter = string.Empty;
        }

        private void LoadCsv_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new Microsoft.Win32.OpenFileDialog
            {
                Title = "Select armor tags CSV",
                Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
                CheckFileExists = true,
                FileName = _vm.CsvPath ?? ""
            };
            if (ofd.ShowDialog(this) == true)
            {
                try
                {
                    _vm.LoadCsvFrom(ofd.FileName);
                    _vm.CsvPath = ofd.FileName;
                    _vm.RefreshAggregateStates();
                }
                catch (Exception ex)
                {
                    System.Windows.MessageBox.Show(this, ex.ToString(), "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void SaveCsv_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string? path = _vm.CsvPath;
                if (string.IsNullOrWhiteSpace(path))
                {
                    var sfd = new Microsoft.Win32.SaveFileDialog
                    {
                        Title = "Save armor tags CSV",
                        Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
                        FileName = "armor_tags.csv",
                        OverwritePrompt = true
                    };
                    if (sfd.ShowDialog(this) == true)
                    {
                        path = sfd.FileName;
                        _vm.SaveCsvTo(path);
                        _vm.CsvPath = path;
                        System.Windows.MessageBox.Show(this, $"Saved: {path}", "Saved");
                    }
                }
                else
                {
                    _vm.SaveCsvTo(path);
                    System.Windows.MessageBox.Show(this, $"Saved: {path}", "Saved");
                }
            }
            catch (Exception ex)
            {
                System.Windows.MessageBox.Show(this, ex.ToString(), "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
        private void UnloadCsv_Click(object sender, RoutedEventArgs e)
        {
            var result = System.Windows.MessageBox.Show(this,
                "This will clear all current selections in memory and unload the CSV. Continue?",
                "Unload CSV", MessageBoxButton.YesNo, MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            _vm.UnloadCsvAndClear();
            //System.Windows.MessageBox.Show(this, "CSV unloaded. All tags reset to unchecked.", "Done");
        }
    }
}
