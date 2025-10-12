using Noggog;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace scscd_gui.wpf
{
    /// <summary>
    /// Interaction logic for Window1.xaml
    /// </summary>
    public partial class ClothingTypesWindow : Window
    {
        public ClothingTypesWindow(DirectoryPath dataDir)
        {
            InitializeComponent();
            DataContext = new ItemTypeManagerVM(dataDir); // swap in your real loader here
        }
    }
}
