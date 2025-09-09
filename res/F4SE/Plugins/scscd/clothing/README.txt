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

CSV files must be placed in `nsfw` or `sfw`. If they are placed outside of either
of those directories, they will not be detected. You are encouraged to put your
CSV files in a subdirectory there, so that they can be combined with other CSVs
targeting the same plugin.

IMPORTANT: The difference between SFW and NSFW is simple but important! If an item
set is bound to an underwear slot (36 on women and 39 on women and men) but
does not visually cover those parts, then it should go under 'nsfw'. Otherwise if
it binds to some other slot (e.g. armor or outerwear) OR if it adequately covers
the bits, then it can go into 'sfw'.

* Why does this work? Let's say you have an item for a woman that leaves the chest
  exposed BUT is bound to slot 41 (outerwear torso). This mod will still go on to
  choose another item to fill the underwear slot 39, thus protecting her modesty
  (according to the mod's settings at least). But if the same item is bound to
  slot 39, then she's going to be flapping in the breeze if she chooses it, and thus
  it becomes NSFW.

This means that if you have a mod that contains some SFW and some NSFW, then you
need TWO .csv files -- one placed in `sfw` and the other in `nsfw`.

This way people only get NPCs walking around half-naked if they opted into that.


FILE FORMAT
===========

CSV files are laid out like so:

Sex Bitmap , Occupation Bitmap , Form ID; Form ID; Form ID
11         , 101000001000      , 123ABC ; ABC123

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

Form ID - This is a SEMICOLON separated list of Form IDs in your mod. Each FormID
          MUST be an Armor type. Together these form IDs build the 'clothing set'
          that will be equipped. Most entries will only specify 1 form ID. If you
          specify multiple, then they form a SET which must be equipped TOGETHER,
          or NOT AT ALL.
          This is useful if you have two separate Armor pieces that need to be
          equipped together to look right. For example, if you have a jacket,
          you might specify the jacket's Form ID. But if you have a harness,
          and part of the harness is cut away so that it doesn't clip with the
          jacket, and the harness may only be worn with the jacket and otherwise
          won't look right, then in that case you'd specify both the jacket and
          harness IDs together.

* You can safely pass a full 8-digit FormID, but only the rightmost
  6 digits (3 digits for light plugins) will be respected. This is
  because the first 2 digits (4 for light) vary at runtime depending on load
  order. Therefore SCSCD will determine the correct digit values at load time.
  Passing a full 8-digit form ID is therefore a convenience; it's super easy to
  copy/paste from FO4Edit this way, and has no drawback since SCSCD performs
  the correction dynamically.

* What about race? - Racial compatibility is defined pretty well within the
  mod plugin .ESP file itself, so SCSCD is able to look that up dynamically
  when the game starts. Since a mistake here means the item CAN'T actually be
  used, and there'd be no point to (unsuccessfully) equipping it, it made more
  sense to go this route than to encode a racial bitmap. Plus it makes the CSV
  files a LOT easier to manage.


FORM IDS
========

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


CSV FILE FORMAT RULES
=====================

CSV columns are in fixed order; don't rearrange them. The first column MUST
be sex, the second MUST be occupation, and the third MUST be form IDs.

All fields will ignore white space, so you can format the files as you like.

The '#' sign will signal a comment, and nothing on that line following the '#'
will be processed. Multiline comments must use one '#' per line.

The first line should be a header, to keep things clear for the maintainer.
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
