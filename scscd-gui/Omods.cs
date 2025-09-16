using Mutagen.Bethesda;
using Mutagen.Bethesda.Fallout4;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Cache;
using Mutagen.Bethesda.Plugins.Order;
using Mutagen.Bethesda.Plugins.Records;
using System;
using System.Collections.Generic;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace scscd_gui.wpf
{
    public class Omods {

        // Return OMODs compatible with an ARMO by "Attach Parent Slots" overlap.
        // Optionally pass linkCache to follow TemplateArmor if slots are inherited.
        public static IReadOnlyList<IAObjectModificationGetter> FindCompatibleOmodsForArmor(
            IArmorGetter armo,
            IEnumerable<IAObjectModificationGetter> allOmods,
            ILinkCache? linkCache = null)
        {
            var armoSlots = EffectiveAttachParentSlots(armo, linkCache).ToHashSet();
            if (armoSlots.Count == 0) return [];

            var hits = allOmods.Where(om => {
                return (
                    armoSlots.Contains(om.AttachPoint.FormKey)
                    && armo.Keywords != null
                    && om.TargetOmodKeywords != null
                    && om.TargetOmodKeywords.Any(tkw => armo.Keywords.Contains(tkw))
                    // only include omods that appear in the same mod, to exclude possible
                    // slot matches that are not compatible. Unfortunately this will also
                    // exclude slot matches that ARE compatible but since the main goal
                    // for omods here is mat swaps, this should be an acceptable compromise.
                    && om.FormKey.ModKey == armo.FormKey.ModKey
                );
            });

            return hits.ToList();
        }

        public static void DumpAllOmodsCsv(IEnumerable<IModListingGetter<IFallout4ModGetter>> activeLo, string outPath)
        {
            // Build from the *active* load order only:
            var omods = activeLo.AObjectModification()
                                .WinningOverrides()
                                .ToList();

            using var sw = new StreamWriter(outPath, false);
            // header
            sw.WriteLine("Plugin,FormKey,EditorID,AttachParentSlots,FilterKeywords");

            foreach (var om in omods)
            {
                var plugin = om.FormKey.ModKey.FileName;
                var formKey = om.FormKey.ToString();
                var edid = om.EditorID ?? "";

                var slots = string.Join(';',
                    (om.AttachParentSlots ?? Enumerable.Empty<IFormLinkGetter<IKeywordGetter>>())
                    .Where(k => !k.IsNull)
                    .Select(k => k.FormKey.ToString()));

                var filters = string.Join(';',
                    (om.FilterKeywords ?? Enumerable.Empty<IFormLinkGetter<IKeywordGetter>>())
                    .Where(k => !k.IsNull)
                    .Select(k => k.FormKey.ToString()));

                // CSV-safe (quote if needed)
                static string Csv(string s) => s.Contains(',') || s.Contains('"')
                    ? $"\"{s.Replace("\"", "\"\"")}\"" : s;

                sw.WriteLine(string.Join(",",
                    Csv(plugin), Csv(formKey), Csv(edid), Csv(slots), Csv(filters)));
            }
        }

        // Optional: also require OMOD FilterKeywords to exist on the ARMO.
        public static bool PassesFilterKeywords(IAObjectModificationGetter om, IArmorGetter armo)
        {
            var filters = om.FilterKeywords;
            if (filters is null || filters.Count == 0) return true;

            var armoKw = new HashSet<FormKey>(
                (armo.Keywords ?? Enumerable.Empty<IFormLinkGetter<IKeywordGetter>>())
                    .Where(k => !k.IsNull).Select(k => k.FormKey));

            // Change All→Any if you prefer "at least one" match.
            return filters.All(k => !k.IsNull && armoKw.Contains(k.FormKey));
        }

        // Helper: read ARMO.AttachParentSlots; if empty, follow TemplateArmor chain.
        private static IEnumerable<FormKey> EffectiveAttachParentSlots(IArmorGetter armo, ILinkCache? cache)
        {
            var slots = armo.AttachParentSlots ?? Enumerable.Empty<IFormLinkGetter<IKeywordGetter>>();
            var set = slots.Where(k => !k.IsNull).Select(k => k.FormKey).ToList();
            Debug.WriteLine($"{set.Count} in set");
            foreach (var n in set)
                Debug.WriteLine($"{armo.EditorID}: {n}");
            if (set.Count > 0 || cache is null) return set;

            if (!armo.TemplateArmor.IsNull && armo.TemplateArmor.TryResolve(cache, out var templ))
                return EffectiveAttachParentSlots(templ, cache);

            return Enumerable.Empty<FormKey>();
        }
    }
}
