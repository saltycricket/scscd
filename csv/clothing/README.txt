BASICS
======

CSV files must be named after the mod that they apply to. For example, if you have
a mod called MySpiffyClothingMod.esp, then you need a MySpiffyClothingMod.esp.csv.

CSV files must be placed in `nsfw` or `sfw`. If they are placed outside of either
of those directories, they will not be detected.

IMPORTANT: The difference between sfw and nsfw is simple but important! If an item
set is bound to an underwear slot (36 on women and 39 on women and men) but
does not visually cover those parts, then it should go under 'nsfw'. Otherwise if
it binds to some other slot (e.g. armor or outerwear) or if it covers the bits, then
it can go into 'sfw'.

* Why does this work? Let's say you have an item for a woman that leaves the chest
  exposed BUT is bound to slot 41 (outerwear torso). This mod will still go on to
  choose another item to fill the underwear slot 39, thus protecting her modesty
  (according to the player's settings at least). But if the same item is bound to
  slot 39, then she's going to be flapping in the breeze if she chooses it, and thus
  it becomes NSFW.

This means that if you have a mod that contains some SFW and some NSFW, then you
need TWO .csv files -- one placed in `sfw` and the other in `nsfw`.

This way people only get NPCs walking around half-naked if they opted into that.


FILE FORMAT
===========

CSV files are laid out like so:

Race Bitmap , Sex Bitmap , Occupation Bitmap , Form ID; Form ID; Form ID
0001010000  , 11         , 101000001000      , 123ABC ; ABC123

Race Bitmap - This defines all the possible races the item can be worn by. For
              example, if both Human Adult and Ghoul Adult can equip it, then
              you need to set the 'Human Adult' bit and the 'Ghoul Bit' to 1,
              leaving all others 0 which means it can't be worn by others.

Sex Bitmap - Same rules apply but there's only two bits. '10' means female, '01'
             means male, '11' means both, '00' means neither (but why?).

Occupation Bitmap - Same rules apply.

Form ID - This is a SEMICOLON separated list of Form IDs in your mod. Each FormID
          MUST be an Armor type. Together these form IDs build the 'clothing set'
          that will be equipped. Most entries will only specify 1 form ID. If you
          specify multiple, then they will be equipped together, or not at all.
          This is useful if you have two separate Armor pieces that need to be
          equipped together to look right. For example, if you have a jacket,
          you might specify the jacket's Form ID. But if you have an undershirt,
          and part of the undershirt is cut away so that it doesn't clip with the
          jacket, and the undershirt may only be worn with the jacket and otherwise
          won't look right, then in that case you'd specify both the jacket and
          undershirt IDs together.

RULES:

All fields will ignore white space, so you can format the files as you like.

Lines beginning with ; will be ignored, so you can leave comments.

All bitmaps are fixed length. They must contain EXACTLY the correct number of
digits, or a warning will be logged and the entry will be ignored. Luckily to
fix the error you can simply exit the game, edit the file, then restart the game.

Valid races (length: 10):

    1000000000 => ALIEN
    0100000000 => DOG
    0010000000 => FERAL
    0001000000 => GHOUL_ADULT
    0000100000 => GHOUL_CHILD
    0000010000 => HUMAN_ADULT
    0000001000 => HUMAN_CHILD
    0000000100 => SUPERMUTANT
    0000000010 => SYNTH_GEN1
    0000000001 => SYNTH_GEN2
    Example (Human Adult + Ghoul Adult): 0001010000

Valid sexes (length: 2):
    01 => Male
    10 => Female
    11 => Both

Valid Occupations (length: 12):
    100000000000 => CITIZEN
    010000000000 => CULTIST
    001000000000 => DRIFTER
    000100000000 => FARMER
    000010000000 => GUARD
    000001000000 => RAIDER
    000000100000 => SCIENTIST
    000000010000 => SOLDIER
    000000001000 => SPY
    000000000100 => VAULTDWELLER
    000000000010 => CAPTIVE
    000000000001 => DOCTOR
    Example (Citizen + Drifter + Spy): 101000001000

