====================
SCSCD: How To Do It!
 (Clothings Edition)
====================

It's really not as complicated as it sounds. Promise. But, please read this file
before asking questions.


BASICS
======

CSV files must be named after the mod that they apply to. For example, if you have
a mod called MySpiffyClothingMod.esp, then you need a MySpiffyClothingMod.esp.csv.

CSV files must be placed beneath this directory. If they are placed outside of the
directory containing this README, they will not be detected. You are encouraged to
put your CSV files in a subdirectory there, so that they can be combined with other
CSVs targeting the same plugin.


CSV FILE FORMAT RULES
=====================

CSV columns are in fixed order; don't rearrange them. The first column MUST
be sex, the second MUST be occupation, and the third MUST be form IDs. The
rest are optional, but if present they MUST be MinLevel, OMODs and NSFW, in
that order.

All fields will ignore white space, so you can format the files as you like.

The '#' sign will signal a comment, and nothing on that line following the '#'
will be processed. Multiline comments must use one '#' per line.

The first line should be a header, to keep things clear for the maintainer.

The GUI tool that ships with this mod assumes the first non-comment line to be
a header, and skips it.

The first column of the first line should be "Sex". It's not case sensitive
and the parser will see that and realize it's a header, and not try to process
it.

All bitmaps are fixed length. They must contain EXACTLY the correct number of
digits, or a warning will be logged and the entry will be ignored. Luckily to
fix the error you can simply exit the game, edit the file, then restart the game.

There are (currently) no OVERRIDES and no EXCEPTION LISTS. This means that
multiple entries of the same Form ID and different Occupations/Sexes will be
MERGED. If one line specifies Gunner and another line specifies Raider, then
that specific clothing will be used for both Gunners and Raiders.

Valid sexes (length: 2):
    01 => Male
    10 => Female
    11 => Both

Valid Occupations (length: 20):
    10000000000000000000 => RAILROAD_RUNAWAY
    01000000000000000000 => RAILROAD_AGENT
    00100000000000000000 => MINUTEMAN
    00010000000000000000 => INSTITUTE_SOLDIER
    00001000000000000000 => INSTITUTE_ASSASSIN
    00000100000000000000 => GUNNER
    00000010000000000000 => RAIDER
    00000001000000000000 => BOS_SOLDIER
    00000000100000000000 => BOS_SUPPORT
    00000000010000000000 => MERCHANT
    00000000001000000000 => CITIZEN
    00000000000100000000 => CULTIST
    00000000000010000000 => DRIFTER
    00000000000001000000 => FARMER
    00000000000000100000 => GUARD
    00000000000000010000 => SCIENTIST
    00000000000000001000 => MERCENARY
    00000000000000000100 => VAULTDWELLER
    00000000000000000010 => CAPTIVE
    00000000000000000001 => DOCTOR

    Example (Minuteman + Citizen + Drifter + Mercenary): 00100000001010001000

    This example would indicate that the entry can be worn by minutemen,
    citizens, drifters, and mercenaries; and unless the same form appears
    again in another line, cannot be worn by anyone else.


If you have any difficulty, please:

1. Check the stock files that came with the base mod. There are plenty of
   examples, and occasionally some comments.

2. Reach out for help on the forum where you downloaded SCSCD.


CSV COLUMNS
===========

CSV files are laid out like so:

Sex Bitmap , Occupation Bitmap , ID; ID; ...     , MinLevel , OMODs , NSFW
11         , 101000001000      , 123ABC ; ABC123 , 10       ,       , true

Sex Bitmap - Defines which sexes can wear the item. Must be exactly two
             characters. The value is a bitmap, so a '1' means enabled and a
             '0' means disabled. A value of '10' means female, a value of '01'
             means male, and a value of '11' means both female and male.
             (Theoretically, a value of '00' would mean neither, but that
             would effectively disable the entry.)

Occupation - Same rules apply. Exactly 20 characters. See the bottom of this
Bitmap     - file for what each character in the bitmap means. I find the
             easiest way to generate these is to put the occupation list on
             the left and my editor on the right. Read down the list on the
             left, and if the character would equip the item, hit '1'.
             Otherwise hit '0'. Then move down the list until you've filled
             all 20 characters. After a few of these you'll get quite fast.

ID - This is a SEMICOLON separated list of Form IDs or Editor IDs in your mod.
     Editor ID is typically preferred, but is not always possible to use.
     Editor ID is subject to conflicts across all mods; however, it will
     survive the compaction process if a user converts the mod to ESL or if
     they merge it with other mods. Form ID is guaranteed not to conflict
     with any other mods (overridden perhaps, but no one value can represent two
     different entities), BUT will not survive the compaction process if the
     mod has been merged or converted to ESL.
     * The IDs **MUST** refer to an Armor type.
     * Notice that multiple IDs may be specified: together these IDs build the
       'clothing set' that will be equipped. Most entries will only specify a
       single ID. If you specify multiple, then they form a SET which must be
       equipped TOGETHER, or NOT AT ALL. This is useful if you have two
       separate Armor pieces that need to be equipped together to look right.
       For example, if you have a jacket, you might specify the jacket's Form
       ID. But if you have a harness, and part of the harness is cut away so
       that it doesn't clip with the jacket, and the harness may only be worn
       with the jacket and otherwise won't look right, then in that case you'd
       specify both the jacket and harness IDs together.
     * You can safely pass a full 8-digit FormID, but only the rightmost
       6 digits (3 digits for light plugins) will be respected. This is
       because the first 2 digits (4 for light) vary at runtime depending on load
       order. Therefore SCSCD will determine the correct digit values at load time.
       Passing a full 8-digit form ID is therefore a convenience; it's super easy to
       copy/paste from FO4Edit this way, and has no drawback since SCSCD performs
       the correction dynamically.
     * This runtime lookup of the correct plugin is not possible with Editor IDs,
       which is why they can conflict with other plugins. You should take every
       reasonable step to use distinct Editor IDs if you plan to go this route.

MinLevel - The minimum level at which the clothing can be equipped to an NPC.
           If left blank, the value is 0. Most of the time, 0 is correct:
           clothing doesn't typically affect game balance. If your clothing
           is particularly high tech-looking, or does introduce mechanics
           that affect game balance, then consider assigning an appropriately
           high MinLevel to it.

OMODs - If your clothing supports armor attachments (typically material swaps),
        list them here. Again, you can use Editor ID or Form ID, and all of the
        tradeoffs and limitations already discussed apply here as well.
        OMODs should be semicolon-separated. Unlike ID, multiple OMODs here
        means "choose one", not "all-or-nothing". At sample time, when an armor
        is chosen, if it supplies any OMODs, one of the OMODs will be chosen
        at random. Note that if 1+ OMODs are specified, it is not possible for
        the armor to be generated with no OMODs attached to it.

NSFW - Flags whether the clothing in question should be considered NSFW. If
       omitted or false (or 0), the clothing is considered SFW. Otherwise, it
       is considered NSFW. **Please note** that this mod has a very specific
       and technical definition of NSFW, as follows:
       * If an item set is bound to an underwear slot (36 on women and 39 on
         women and men) but does not visually cover those parts, then it should
         be flagged 'nsfw'. Otherwise if it binds to some other slot (e.g. armor
         or outerwear) OR if it adequately covers the bits, then it does not
         need the 'nsfw' flag.
       * Why does this work? Let's say you have an item for a woman that leaves
         the chest exposed BUT is bound to slot 41 (outerwear torso). This mod
         will still go on to choose another item to fill the underwear slot 39,
         thus protecting her modesty (according to the mod's settings at least).
         But if the same item is bound to slot 39, then she's going to be
         flapping in the breeze if she chooses it, and thus it becomes NSFW.
       * This way people only get NPCs walking around half-naked if they opted
         into that.

* What about race? - Racial compatibility is defined pretty well within the
  mod plugin .ESP file itself, so SCSCD is able to look that up dynamically
  when the game starts. Since a mistake here means the item CAN'T actually be
  used, and there'd be no point to (unsuccessfully) equipping it, it made more
  sense to go this route than to encode a racial bitmap. Plus it makes the CSV
  files a LOT easier to manage.


USING FORM IDS
==============

In general, if you can use Editor IDs, you'll have an easier time of it. But
if you find for some reason that you need to use Form IDs (use cases do exist),
then read this section to understand how they work within these CSV files and
how this mod processes them.

So there's a fair amount of complexity to this. I've been able to abstract most
of the gotcha's away so that you don't have to think about them. If you are
creating a .CSV file for a regular .ESP plugin, then you really don't have to think
at all. Just enter in the form ID and move on. Skip this whole section. Really,
your brain cells will thank you.

Likewise, if you've created an **ESL-flagged ESP**, for example by right-clicking
the ESP in Vortex and selecting "Make Light", then once again you can continue
on your merry way, because this operation only actually works when the form IDs
are consistent. My mod will handle the load order, ESL bit twiddling, etc. Just
do your thang.

But things get interesting when you use COMPACTED FORM IDs. This only applies
to someone who opens FO4Edit, right-clicks their mod, and selects the
"Compact FormIDs for ESL" option. When you do this, you are actually re-writing
the form IDs. This will break compatibility with existing saves and with any
mod that depends on the ESP you are modifying. Because the form IDs will have
been changed, you will need to create a new CSV file to match. So, if you
are the **original author** of such a mod and you intend to release it as an
ESL, then wait to create the .CSV file until you've created the .ESL file
first. It'll save you from night terrors.

On the other hand, if you're **not** the original mod author, and you're
creating a .CSV to adapt somebody's existing work, AND you've compacted the
original mod's form IDs in this way, then for best compatibility, I **highly**
recommend you rename the file to .ESL, and create a .ESL.CSV file to match.
This way end users can use your .ESL.CSV file alongside .ESP.CSV files for the
unaltered original mod, if they prefer. SCSCD will be smart enough to check
whether any given set of form IDs are actually created by the ESP/ESL file
being processed, so if one is present and the other is missing, everything will
Just Work(TM).

If you are stubborn and, after compacting form IDs, you still absolutely refuse
to rename the .ESP to .ESL (hey I get it, that's me, for Reasons), then you will
need to either drop support for the original forms and bundle the compacted .ESP
file with your CSV files (and warn your users that their saved games may be broken
by your changes!) -- OR, you can try to include BOTH the original and the
compacted form IDs in one .ESP.CSV file. If you do this, you will generate warnings
in the end user's log file (because one set of forms will load and the other
will be invalid) - but otherwise it "Should be fine"(TM).

