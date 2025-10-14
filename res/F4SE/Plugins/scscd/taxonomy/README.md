# SCSCD: How To Do It!
# (Taxonomy Edition)

This set of CSVs map Clothing Types to the underlying SCSCD slot layout.


## Background

The original version of SCSCD tried to let all of the clothing mods get along;
it hoped that the mod authors at least _tried_ to follow a community standard
(such as it is). Unfortunately there was too much ambiguity in what little
standard exists, and more unfortunately, they don't appear to have tried all
that hard. Consequently we ended up with a metric ton of clipping problems.

Eventually I figured out how to alter the slot bindings within the engine,
at runtime. I won't bore you with the details. The bottom line is that to
improve compatibility, SCSCD takes 'ownership' of the slot layout. I defined
my own slot layout (heavily biased toward the Vanilla FO4 layout for best
compatibility), and clothing types can now -- optionally -- be mapped to that
layout.

The general CSV rules follow the Clothings: First line is a header, comments
are '#', case insensitive, and white space is trimmed. Read that README for
details.


## How It Works

At run-time, ALL CSVs in the `taxonomy` directory (and subdirectories) are
scanned and loaded. It is expected and assumed that every entry in these files
is UNIQUE. Duplicates will conflict and one will be dropped in favor of the
other (no guarantees are made).

When a clothing item specifies a Clothing Type, its slot layout will be
adjusted to match the layouts in the matching CSV entry.

That's pretty much it.


## CSV Column Descriptions

Name - The clothing type name. Clothings which opt-in to the remapping (it is
       optional) MUST use this name. It MUST match exactly! It MUST be unique!

ARMO Slots - The equip slots. This is what you see in your (or the NPC's)
             inventory screen. It's the easiest to understand: items that
             conflict simply can't be equipped together.

ARMA Slots - The _suppression_[1] slots. (At least that's how I think of them.)
             This is a list of slots that can be drawn to, ONLY by the item
             in question. For example, if you have an overshirt that also
             specifies the "bra" suppression slot, then any bra (if equipped)
             will not be rendered. Useful if you have a tight-fitting shirt
             that clips with bras. A better example might be a helmet; you
             might want to supress the hair, so that it doesn't clip through
             the helmet.

[1] Actually, the game treats this as a _mutally-exclusive render list_. It
    will allow THIS item to render, but no OTHER items sharing the same slots
    will be rendered. SCSCD automatically merges ARMO slots into ARMA slots,
    so you don't have to specify ARMO slots more than once. The game engine
    internally does require that, or else chaos ensues.
