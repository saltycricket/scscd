using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;

namespace scscd_gui.wpf
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _vm = new();

        public MainWindow()
        {
            InitializeComponent();
            DataContext = _vm;
        }

        private async void Window_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                await _vm.InitializeAsync();
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
