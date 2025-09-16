======================
SCSCD: How To Do It!
 (Occupations Edition)
======================

This set of CSVs map classes, factions, and specific NPCs to Occupations.

As with Clothings, all values are ADDITIVE. If an entry appears on more
than one line, the target will be capable of EITHER/ANY occupation that it
is registered to. Precisely which occupation influences its clothing will be
chosen at run-time, when the actor is first loaded.

The general CSV rules follow the Clothings: First line is a header, comments
are '#', case insensitive, and white space is trimmed. Read that README for
details.

There are 2 columns: [ Occupation ], [ ID ]. The ID is either a Form ID or
an Editor ID. Using its EditorID is obviously more convenient but it is also
much less precise: EditorID is subject to conflicts across different plugins
(and sometimes within the same plugin). So, FormID is generally safer, but
EditorID has wider reach due to working across compacted and merged plugins.

**Note** that as of this writing, NPCs cannot be found by their EditorID. (It
seems the engine may strip it, probably for performance or to reduce memory
footprint.) So, NPCs must always be specified by their FormID.

You should go ahead and take a look at `scscd_official/Fallout4.esm.csv`
for a detailed example. This file obviously targets occupations for vanilla
entities. Other DLCs, mods, etc will be mapped in due time.

An ID can be assigned to more than one Occupation. If it is,
the actual match will be chosen randomly at runtime. The ID
can belong to a Class, Faction or NPC.

You can safely pass a full 8-digit FormID, but only the rightmost
6 digits (3 digits for light plugins) will be respected. This is
because the first 2 digits vary at runtime depending on load order.
Therefore SCSCD will determine the correct digit values at load time. Passing
a full 8-digit form ID is therefore a convenience; it's super easy to
copy/paste from FO4Edit this way, and has no drawback.

TODO support weighted random occupation so some occupations can be more
likely for some entities.

Possible occupations include:

    railroad_runaway
    railroad_agent
    minuteman
    institute_soldier
    institute_assassin
    gunner
    raider
    bos_soldier
    bos_support
    merchant
    citizen
    cultist
    drifter
    farmer
    guard
    scientist
    mercenary
    vaultdweller
    captive
    doctor

Although you can assign any class, faction or NPC to any occupation
and you can assign them to more than one, some rules of thumb for
non-humans (treat non-feral ghouls as humans):

    aliens - drifter
    dogs - drifter
    children - citizen
    supermutants - guard
    feral ghoul - drifter
