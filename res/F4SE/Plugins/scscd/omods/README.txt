======================
SCSCD: How To Do It!
 (OMODs Edition)
======================

This set of CSVs map armors/clothings to supported OMODs. The primary use case
is to support material swaps, but others like linings, flashlights, etc are
possible this way too.

You should read the Clothing README.txt first. It is your intended starting
point.

That said, OMODs is probably the easiest CSV to understand, and definitely the
easiest to author.

The first column is the ARMO form IDs; the second column is the OMOD form IDs.
The CSV simply maps one to the other. Semicolons here mean "any of these armors"
can be equipped with "any of these omods". It's a many-to-many relationship.
UNLIKE Clothing CSVs, which specify armors in EXCLUSIVE GROUPS (all or nothing),
OMOD CSVs specify armors in NON-EXCLUSIVE GROUPS (use one of any of these).


HOW TO AUTHOR

Just open your mod FO4Edit, select "Keywords", click on a
Keyword that represents the armor-to-omod mapping, and click "Referenced By".
You will see ARMO and OMOD entries. Sort by Signature, so that all ARMO are on
the top. Now select all ARMO, copy, and paste into your text editor. Put a
comma, then copy and paste the OMOD entries. You'll have something like:

    ARMO1
    ARMO2 , OMOD1
    OMOD2

Now just collapse these lines into 1 line, adding a semicolon at the end of
each line. Result:

    ARMO1;ARMO2 , OMOD1;OMOD2

Done. Really, super fast and easy. Less than 30 seconds per armor if you have
an editor that lets you edit multiple lines, like Sublime Text does.

With this mapping, SCSCD can pick a random OMOD and attach it to your armor
when it's equipped.

If OMOD CSVs are omitted, it won't prevent the clothing from being used. It'll
just spawn with no OMODs. Default material, no addons. Boring but fine.

LIMITATION: Some mods specify material swaps but not OMODs. As far as I can
tell this means the armor can be spawned with any of those swaps, but cannot
have the swap applied from a workbench. I started to explore supporting this
but gave up. These kinds of mods will just have to use whatever they spawn with,
and I'm not sure if that means the matswaps will be applied automatically by
the engine, or if they will always use a base material. In any case they seem
few and far between.

TODO/FIXME: It should be theoretically possible for SCSCD to scan the Keywords
at runtime and automatically generate this list, making OMOD CSVs unnecessary.
However, some key information seems to be missing from CommonLibF4 at this time,
and I haven't been successful in doing this to date. If this information can be
found, restored or worked around, then OMOD CSVs might become obsolete.
