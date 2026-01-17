 W3X\_Files\_Format     

[☰](javascript:void(0);)

[Home](/) [Tag](/tag) [Search](/search) [StartPage](/startpage/)

*   [Introduction](#introduction)
*   [Things you need to know first!](#things-you-need-to-know-first)
    *   [Your environment](#your-environment)
    *   [Warcraft 3 Files](#warcraft-3-files)
        *   [About MPQ Files](#about-mpq-files)
        *   [Warcraft III File Structure](#warcraft-iii-file-structure)
        *   [Map files (W3M/W3X Files)](#map-files-w3mw3x-files)
    *   [Warcraft 3 Data Format](#warcraft-3-data-format)
        *   [Integers](#integers)
        *   [Short Integers](#short-integers)
        *   [Floats](#floats)
        *   [Chars and Array of Chars](#chars-and-array-of-chars)
        *   [Trigger Strings and Strings](#trigger-strings-and-strings)
        *   [Flags](#flags)
        *   [Custom Types](#custom-types)
        *   [Structures](#structures)
*   [W3M/W3X Files Format](#w3mw3x-files-format)
*   [war3map.j](#war3mapj)
*   [war3map.w3e](#war3mapw3e)
*   [war3map.shd](#war3mapshd)
*   [war3mapPath.tga/war3map.wpm](#war3mappathtgawar3mapwpm)
    *   [war3mapPath.tga](#war3mappathtga)
    *   [war3map.wpm](#war3mapwpm)
*   [war3map.doo](#war3mapdoo)
*   [war3mapUnits.doo](#war3mapunitsdoo)
*   [war3map.w3i](#war3mapw3i)
*   [war3map.wts](#war3mapwts)
*   [war3mapMap.blp](#war3mapmapblp)
*   [war3map.mmp](#war3mapmmp)
*   [war3map.w3u / w3t / w3b / w3d / w3a / w3h / w3q](#war3mapw3u--w3t--w3b--w3d--w3a--w3h--w3q)
*   [war3map.wtg](#war3mapwtg)
*   [war3map.w3c](#war3mapw3c)
*   [war3map.w3r](#war3mapw3r)
*   [war3map.w3s](#war3mapw3s)
*   [war3map.wct](#war3mapwct)
*   [war3map.imp](#war3mapimp)
*   [war3map.wai](#war3mapwai)
*   [war3mapMisc.txt, war3mapSkin.txt, war3mapExtra.txt](#war3mapmisctxt-war3mapskintxt-war3mapextratxt)
*   [W3N File Format](#w3n-file-format)
*   [war3campaign.w3f](#war3campaignw3f)
*   [SLK files](#slk-files)
*   [BLP files](#blp-files)
*   [MDX and MDL files](#mdx-and-mdl-files)
*   [Packed Files - W3V, W3Z, W3G](#packed-files---w3v-w3z-w3g)
*   [Campaigns.w3v](#campaignsw3v)
*   [W3Z files](#w3z-files)
*   [W3G files](#w3g-files)
*   [The others files](#the-others-files)

 

W3X\_Files\_Format
==================

字数 17559 · 2019-05-09

Last update: 03/02/2020

Introduction
============

This documentation contains almost all the specifications of Warcraft III maps files ( `*.w3m` and `*.w3x` ). This was made without any help from Blizzard Entertainement and did not involve **“reverse engineering”** of the Warcraft III engine. The specification of each kind of file depend on its version. I did document here the current version used by Warcraft III Reforged; make sure the file you’re looking at or modifying are using the same version/format as describbed.

Things you need to know first!
==============================

Your environment
----------------

You’ll need Warcraft III Retail installed let’s say in `"C:\Program Files\Warcraft III\"`. If your Warcraft III installation is clean, you should have these files in your `"C:\Program Files\Warcraft III\"`:

*   `war3.mpq`
*   `Maps\(4)Lost Temple.w3m`  
    If you have also installed the expansion The Frozen Throne, you’ll have other archives that hold the new files. These are:
*   `war3x.mpq`
*   `war3xlocal.mpq`
*   `Maps\FrozenThrone\(2)Circumvention.w3x`
*   `Campaigns\DemoCampaign.w3n`  
    Also by installing updates a new archive will be added that holds the most up-to-date files from the patch, namely
*   `war3patch.mpq`

`"W3M"` and `"W3X"` and also map files as well as `"W3N"` campaign files can be opened with any **MPQ editor** that supports Warcraft 3.

You’ll need one. I suggest Ladik’s MPQ Editor, you can get it here:

> [http://www.zezula.net/en/mpq/download.html](http://www.zezula.net/en/mpq/download.html)

If you want to “play” with the map files, you’ll also need a hexadecimal editor. My favorite one is **HxD** ([https://mh-nexus.de/en/hxd](https://mh-nexus.de/en/hxd)).

Warcraft 3 Files
----------------

### About MPQ Files

MPQ are like `"zip"` files as they contain a directory structure with compressed files.  
I’ll not talk about the MPQ format here since Quantam did it already. If you want to know more about it I suggest you go there:

> [http://www.zezula.net/en/mpq/main.html](http://www.zezula.net/en/mpq/main.html)

If you want to edit MPQ archives I suggest you get SFmpqapi for your favourite programming language. It’s available at:

> [https://sfsrealm.hopto.org/downloads/SFmpqapi.html](https://sfsrealm.hopto.org/downloads/SFmpqapi.html)

You can also download the source code of WinMPQ there to learn how to use SFmpqapi.

### Warcraft III File Structure

When it’s looking for a file, Warcraft III looks first in the “real” directories (the one you see in Windows Explorer) if you set up a specific registery key which is:

    1
    2
    3
    4
    Path: HKEY_CURRENT_USER\Software\Blizzard Entertainment\Warcraft III\
    Key name: "Allow Local Files"
    Key type: dword
    Key value: 1
    

If the registery key is not set or the file was not found in the “real” directories, then it looks in your map (`w3m` file), then in the last patch mpq (`War3Patch.mpq`) and finally in the main mpq (`War3x.mpq` or `War3xlocal.mpq` if the expansion is installed) and lately in `War3.mpq`.  
It means that you don’t need to modify official MPQs (**DON’T modify your War3.mpq!**), you just need to use the same directory/file structure in your `"C:\Program Files\Warcraft III\"`.  
Adding files in a map (`.w3m` file) works with most of the files but not all. Remember that the WorldEditor reads the local files from the real directories even if “Allow Local Files” is not enabled in the registry.

It Works (for example) for:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    Units\unitUI.slk
    Units\AbilityData.slk
    UI\MIDISounds.slk
    Units\HumanUnitFunc.txt
    Units\HumanUnitStrings.txt
    Units\HumanAbilityFunc.txt
    Units\HumanAbilityStrings.txt
    Units\HumanUpgradeFunc.txt
    Units\HumanUpgradeStrings.txt
    

But it doesn’t work very well for:

    1
    2
    Units\UnitMetaData.slk
    Scripts\Blizzard.j
    

And it doesn’t work at all for:

    1
    2
    TerrainArt\CliffTypes.slk
    Units\MiscData.txt
    

If you really need to change a file that has to be loaded outside or before a map is loaded, I recommend you create a executable patch with a embedded MPQ archive using `MPQDraft`, which you can download at the above-mentioned link. This option is also used by the third party editors such as `UMSWE`.

_Example:_  
You want to use more than the two default cliff types in a map. By editing the `w3e` file or by using **WE Unlimited** or **Zepir’s Editor** you can technically add more cliff types to your map. The problem is that to have them show up correctly in game it will require a modified `TerrainArt\CliffTypes.slk` file. To make this work you will have to create a new MPQ Archive and import the modified file with the path `TerrainArt\CliffTypes.slk`. Also take care that a listfile entry is added for this file. Then start **MPQDraft** and create a new executable patch. Simply select the created archive as source and select Warcraft III as the target application to patch. That’s it, the patch will be created for you. Now if you start the created executable it will start Warcraft and the modified file will be used instead of the original one.

> **Warning!**
> 
> *   In some cases, if you play with others, everybody will need to have the same modified files or you’ll get an error (like `"netsync error"`).
> *   Some files have a “special” format and if you modify them, you could “falsify” this format. In some cases it will work, in some others it won’t. Be aware of that ‘cause War3 will try to find a “standard file” instead _(the ones of the MPQs instead of yours and you’ll think it didn’t try your stuff)_.
> *   Some files outside both `War3.mpq` and `War3Patch.mpq` will not be used by Warcraft 3. These are exceptions.

### Map files (W3M/W3X Files)

To edit a map, you’ll have to unpack the files of the `"w3m"` somewhere, then modify them and finally put them back in a `"w3m"` file _(usually a new one)_. Since retail, `W3M` are a little bit different from simple `MPQ` files: they got a header and a footer. I’ll talk more about the W3M format in the [“W3M Files Format”](#w3mw3x-files-format) section and the other files inside W3Ms in the following sections.

Warcraft 3 Data Format
----------------------

Blizzard uses several ways to store data in its files. However they often use generic types.

### Integers

Intergers are stored using **4** bytes in **“Little Endian”** order. It means that the first byte read is the lowest byte.  
They are just like the C++ `"int"` _(signed)_ type. In some other documentation of this kind you may see them named `"long"`.  
_Size:_ **4 bytes**  
_Example:_ `1234 decimal` = `[00 00 04 D2]h` will be stored in this order: `[D2 04 00 00]h`

### Short Integers

Short Integers are stored using **2** bytes in **“Little Endian”** order.  
They are close to the C++ signed short but their range is from `-16384` to `16383`. It means the 2 highest bit are free of use for a flag for example.  
_Size:_ **2 bytes**

### Floats

Floats are using the `standard IEEE 32bit float format`. They are stored using **4** bytes and the **“Little Endian”** order.  
They are just like the C++ `"float"` type.  
_Size:_ **4 bytes**  
_Example:_ `7654.32 decimal`, this number can’t be stored using this format so the system will take the closest value that can be represented using binary digits. The closest one is: `7654.319824 decimal` = `[45 EF 32 8F]h` and will be stored as `[8F 32 EF 45]h`

### Chars and Array of Chars

They are just stored like standard chars (1 char = 1 byte) and array of chars _(no null terminating char needed)_.  
_Size (chars):_ **1 byte**  
_Size (array of chars):_ usually **4 bytes**

### Trigger Strings and Strings

Strings are just arrays of chars terminated with a null char (C++ `'\0'`). However Blizzard sometimes use special control codes to change the displayed color for the string. These codes are like `"|c00BBGGRR"` where `"BB"`, `"GG"` and `"RR"` are hexadecimal values _(using 2 digits each)_ for the **blue**, the **green** and the **red** values. If a string starts with `"TRIGSTR_"` _(case sensitive)_, it’s considered as a trigger string. A trigger string is kept in memory as is _(“TRIGSTR\___\*”)_ and is only changed when Warcraft 3 needs to display it. Instead of just writing `"TRIGSTR_000"` on the user screen, War3 will look in its trigger string table created when the map was loaded and display the corresponding trigger string instead. Trigger strings only work for files inside a `w3m` (`Jass`, `w3i`, …) except for the WTS which is used to define the trigger string table itself. If the number following `"TRIGSTR_"` is negative the trigger string will refer to a null _(empty)_ string, if `"TRIGSTR_"` is followed by text, it’ll be considered as trigger string `#0` ( = “TRIGSTR\_000”).  
`"TRIGSTR_7"`, `"TRIGSTR_07"`, `"TRIGSTR_007"` and `"TRIGSTR_7abc"` are all representing trigger string `#7`. `"TRIGSTR_ab7"`, `"TRIGSTR_abc"` and `"TRIGSTR_"` refer to trigger string `#0`. `"TRIGSTR_-7"` is negative and will not refer to a trigger string; it’ll be displayed as `""`. By convention, `"TRIGSTR_"` is followed by 3 digits and the null char that ends the string.

*   _Example 1:_ your got the string \*“blah
    
    c000080FFblah”_, War3 will display “blah blah” but the second “blah” will be orange \*(blue=00 + green=80 + red=FF ==> orange)_.
    
*   _Example 2:_ you got “TRIGSTR\_025” and trigger string 25 is defined (in the .wts file) as “blah
    
    c000080FFblah”, it’ll display the same result as the previous example.
    

_Size (string):_ vary. String length + 1 (null terminating char)  
_Size (trigger string):_ **12 bytes**

For its strings Warcraft uses a unicode format called `UTF-8`. They do this because the files need to be localized into many different languages. This format uses one byte for the most common characters which is equal to the character’s ASCII value. For example `A` = `65` or `0x41`. For the more unusual characters it can take from 2 to 6 bytes per character. For example the German letter would be represented by `195` and `164` or `0xC3A4`. The higher the first byte is, the more bytes are required to represent the character. Simple modulo calculations are enough to convert UTF to common unicode (UCS) and back. To convert UTF to UCS use this pattern:

    1
    2
    3
    4
    5
    6
    If FirstByte <= 191 return FirstByte
    If 192 <= FirstByte <= 223 return (FirstByte - 192) * 64 + (SecondByte - 128)
    If 224 <= FirstByte <= 239 return (FirstByte - 224) * 4096 + (SecondByte - 128) * 64 + (ThirdByte - 128)
    If 240 <= FirstByte <= 247 return (FirstByte - 240) * 262144 + (SecondByte - 128) * 4096 + (ThirdByte - 128) * 64 + (FourthByte - 128)
    If 248 <= FirstByte <= 251 return (FirstByte - 248) * 16777216 + (SecondByte - 128) * 262144 + (ThridByte - 128) * 4096 + (FourthByte - 128) * 64 + (FifthByte - 128)
    If 252 <= FirstByte return (FirstByte - 252) * 1073741824 + (SecondByte - 128) * 16777216 + (ThirdByte - 128) * 262144 + (FourthByte - 128) * 4096 + (FifthByte - 128) * 64 + (SixthByte - 128)
    

To convert UCS back to UTF use this:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    if ASCII <= 127:
      FirstByte = ASCII
    if 128 <= ASCII <= 2047:
      FirstByte = 192 + (ASCII / 64) 
      SecondByte = 128 + (ASCII Mod 64)
    if 2048 <= ASCII <= 65535:
      FirstByte = 224 + (ASCII / 4096) 
      SecondByte = 128 + ((ASCII / 64) Mod 64) 
      ThirdByte = 128 + (ASCII Mod 64)
    if 65536 <= ASCII <= 2097151:
      FirstByte = 240 + (ASCII / 262144) 
      SecondByte = 128 + ((ASCII / 4096) Mod 64) 
      ThirdByte = 128 + ((ASCII / 64) Mod 64) 
      FourthByte = 128 + (ASCII Mod 64)
    if 2097152 <= ASCII <= 67108863:
      FirstByte = 248 + (ASCII / 16777216) 
      SecondByte = 128 + ((ASCII / 262144) Mod 64) 
      ThirdByte = 128 + ((ASCII / 4096) Mod 64) 
      FourthByte = 128 + ((ASCII / 64) Mod 64) 
      FifthByte = 128 + (ASCII Mod 64)
    if 67108864 <= ASCII <= 2147483647: 
      FirstByte = 252 + (ASCII / 1073741824) 
      SecondByte = 128 + ((ASCII / 16777216) Mod 64) 
      ThirdByte = 128 + ((ASCII / 262144) Mod 64) 
      FourthByte = 128 + ((ASCII / 4096) Mod 64) 
      FifthByte = 128 + ((ASCII / 64) Mod 64) 
      SixthByte = 128 + (ASCII Mod 64))
    

The conversion will only be needed if you want to display text in your application or write user input to files. For all other purposes you can internally treat the UTF-Strings just like ordinary strings.

### Flags

Flags are boolean values _(true or false, 1 or 0)_. They can be stored using 4 bytes. Each bit is a flag _(4 bytes = 32 bit = 32 flags)_. Blizzard uses integers to store its flags.  
_Size:_ usually 4 bytes

### Custom Types

Sometimes, an integer and one or several flags can share bytes. This is the case in the W3E file format: the water level and 2 flags are using the same group of 4 bytes. How? the 2 highest bit are used for the flags, the rest is reserved for the water level _(the value range is just smaller)_. Sometimes a byte can contain two or more different data.

### Structures

Warcraft 3 also uses structured types of various size.

W3M/W3X Files Format
====================

A `W3M` or `W3X` file is a Warcraft III Map file _(AKA Warcraft III Scenario in the World Editor)_. It’s just a `MPQ` _(using a `"new"` compression format)_ with a **512 bytes** header. Sometimes, for official `W3M` files, it uses a footer of 260 bytes for authentification purposes.

Here is the header format (fixed size = **512 bytes**):

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    char[4]: file ID (should be "HM3W")
    int: unknown
    string: map name
    int: map flags (these are exactly the same as the ones in the W3I file)
    0x0001: 1=hide minimap in preview screens
    0x0002: 1=modify ally priorities
    0x0004: 1=melee map
    0x0008: 1=playable map size was large and has never been reduced to medium
    0x0010: 1=masked area are partially visible
    0x0020: 1=fixed player setting for custom forces
    0x0040: 1=use custom forces
    0x0080: 1=use custom techtree
    0x0100: 1=use custom abilities
    0x0200: 1=use custom upgrades
    0x0400: 1=map properties menu opened at least once since map creation
    0x0800: 1=show water waves on cliff shores
    0x1000: 1=show water waves on rolling shores
    int: max number of players
    

followed by `00` bytes until the **512 bytes** of the header are filled.

Here is the footer format _(optional)_:

    1
    2
    char[4]: footer sign ID (should be "NGIS" == 'sign' reversed)
    byte[256]: 256 data bytes for authentification. 
    

I don’t know how they are used at the moment.

The MPQ part can contain the following files:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    (listfile)
    (signature)
    (attributes)
    war3map.w3e
    war3map.w3i
    war3map.wtg
    war3map.wct
    war3map.wts
    war3map.j
    war3map.shd
    war3mapMap.blp
    war3mapMap.b00
    war3mapMap.tga
    war3mapPreview.tga
    war3map.mmp
    war3mapPath.tga
    war3map.wpm
    war3map.doo
    war3mapUnits.doo
    war3map.w3r
    war3map.w3c
    war3map.w3s
    war3map.w3u
    war3map.w3t
    war3map.w3a
    war3map.w3b
    war3map.w3d
    war3map.w3q
    war3mapMisc.txt
    war3mapSkin.txt
    war3mapExtra.txt
    war3map.imp
    war3mapImported\*.* 
    

We’ll see now what these files stand for.

war3map.j
=========

**The JASS2 Script**

This is the main map script file. It’s a text file and you can open it with notepad.  
Sometimes it’s renamed to `Scripts\war3map.j` by map protectors to keep you away from it.  
The language used is called JASS2 and has been developed by Blizzard. It’s a case sensitive language.  
When you play a map, the jass script is loaded and executed.  
When you select a map in when creating a game Warcraft III will first look up the `"config"` function and execute its code to set up the player slots.  
Then, when the game has started, Warcraft III looks for the function called `"main"` and executes it.

The language uses several keywords as described here:

Key word

Meaning

function

used to define a new fonction

takes

used to define the number of arguments for a fontction

returns

sets the type of the value returned by a function

return

makes a function leaves and returns a value

endfunction

ends a function definition

call

used to call a function that returns nothing

globals

used to define the list of global variables

endglobals

ends the list of global variables definition

local

defines a local variable

set

assigns a value to a variable

if, elseif, else, then, endif

Just an “if-then-else-endif” chain like in Basic.

loop, exitwhen, endloop

used to make loops in the script

constant

defines a constant

type

defines a new type/class

extends

says what the mother type is

native

defines a function header of an external built-in function implemented in Game.DLL

array

used to define array variables

    1
    2
    3
    4
    if (...) then ...
    elseif (...) then ...
    else ...
    endif
    

Only two `"native types"` exists: `"nothing"` and `"handle"`. All the other types are derived _(keyword “extends”)_ from `"handle"`. You can get the full list of types and native functions from the file called `"Scripts\Common.j"` in your `War3.mpq` file. You can assign several types of constant values to the handle type: a generic null value called `"null"`, any interger, any float, any string or trigger string, `"true"` and `"false"`.

Here is the lis of operators recognized by the language:

Operator

meaning

( )

parenthesis for priorities

+

addition (concatenation for strings)

\-

substraction

\*

multiplication

/

division

\=

assignation

\= =, <, <=, >, >=, !=

comparison (equal, lighter, lighter or equal, greater, greater or equal, different)

not

invert a boolean value

and

connects boolean values (both must be true to be true)

or

connects boolean values (one must be true to be true)

\[\]

parenthesis for arrays

The language knows the following six primitive types:

Type

meaning example

boolean

can be true or false local boolean B = true

integer

can hold whole numbers local integer I = 0

real

can hold floating point numbers local real R = 0.25

string

can hold strings, or null local string S = “hello world”

code

can hold pointers to functions, or null local code C = function main

handle

holds pointers to objects, or null local handle H = null

These six types are the only native types, the others are declared in `"Scripts\Common.j"` and are all derived objects from the handle type such as **units**, **items**, **destructables**, **timers**, **triggers** and many more. Handle is the only extendable type of the language. All native functions usable in the map script are also defined in the file `"Scripts\Common.j"`, though you can also use functions from `"Scripts\Blizzard.j"`, which are used by the GUI triggers of the World Editor. The language is also used for AI files in warcraft. In AI scripts you can use functions from `"Scripts\Common.j"` and `"Scripts\Common.ai"`.  
Example of a function definition:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    function myfunction takes nothing returns integer
    local string str = "blah blah blah"
    local integer i
    // comments line
    set i = 0
    loop
     set i = i + 1
     if (i == 27) then
     call DisplayTimedTextToPlayer(GetLocalPlayer(), 0, 0, 60, str)
     endif
     exitwhen i == 30
    endloop
    return i
    endfunction 
    

One interesting fact is that all types have the same length of 4 bytes. This allows “type casting” by abusing hole in the syntax checker that only requires the last return statement of a function to conform the declared return type. Therefore we can use so called return bug exploiters such as the following to do amazing things:

    1
    2
    3
    4
    function Int2Handle takes integer I returns handle
    return I
    return null
    endfunction 
    

    1
    2
    3
    4
    function Handle2Int takes handle H returns integer
      return H
      return 0
    endfunction 
    

Whereas the 4 bytes of booleans, integers and reals hold the actual value of the variable, the other types are only pointers. Strings are pointers to an internal string array, where all strings are stored. Code, handles and all handle derived types are just pointers to functions and objects in memory. This means that we can store every variable in game cache by using the “StoreInteger” function. The game cache was originally inteded for handing over data from one map to another by saving it to a cache file. But nowadays the cache is mainly used as a hash function to write information directly into memory and read it out later. This also enables communication between the map script and ai files.

You can write integers in the following three different ways:

*   **ordinary**: just like in any other language _(65)_
*   **hex**: in hexadecimal form with the 0x preset _(0x3f)_
*   **literal**: as integer literal by putting the character with the equivalent ascii value in single quotes _(‘A’)_

Most ids such as unit ids, item ids, destructable ids and some more are also integer literals, for example ‘Ahbz’. The characters within the single quotes have the same limitations as characters in strings, except that there is no escape character. The character `"\"` is the escape character in strings which is used to place special characters:

> *   `\n` and `\r`: line break
> *   `\"`: to use the `"` character in strings
> *   `\\`: to use the `\` character in strings

Find more comprehensive documentation on JASS and a Syntax Checker at [http://jass.sourceforge.net/doc/](http://jass.sourceforge.net/doc/)

war3map.w3e
===========

**The environment**

This is the tileset file. It contains all the data about the tilesets of the map. Let’s say the map is divided into squares called “tiles”. Each tile has 4 corners. In 3D, we define surfaces using points and in this case tiles are defined by their corners. I call one tile corner a “tilepoint”. So if you want a `256x256` map, you’ll have `257x257` tilepoints. That’s also why a tile texture is defined by each of its four tilepoints. A tile can be half dirt, one quarter grass and one quarter rock for example. The first tilepoint defined in the file stands for the lower left corner of the map when looking from the top, then it goes line by line (horizontal). Tilesets are the group of textures used for the ground. Here is the file format:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    41
    42
    43
    44
    45
    46
    47
    48
    49
    50
    51
    52
    53
    54
    55
    56
    57
    58
    59
    60
    61
    62
    63
    Header:
    - char[4]: file ID = "W3E!"
    - int: w3e format version [0B 00 00 00]h = version 11
    - char: main tileset [TS]
      Tileset	Meaning
      A Ashenvale
      B Barrens
      C Felwood
      D Dungeon
      F Lordaeron Fall
      G Underground
      L Lordaeron Summer
      N Northrend
      Q Village Fall
      V Village
      W Lordaeron Winter
      X Dalaran
      Y Cityscape
      Z Sunken Ruins
      I Icecrown
      J Dalaran Ruins
      O Outland
      K Black Citadel
    
    - int: custom tilesets flag (1 = using custom, 0 = not using custom tilesets)
    - int: number a of ground tilesets used (Little Endian) (note: should not be greater than 16 because of tilesets indexing in tilepoints definition)
    - char[4][a]: ground tilesets IDs (tilesets table)
      Example: "Ldrt" stands for "Lordaeron Summer Dirt"
      (refer to the files "TerrainArt\Terrain.slk" located in your war3.mpq for more details)
    - int: number b of cliff tilesets used (Little Endian) 
      (note: should not be greater than 16 because of tilesets indexing in tilepoints definition)
    - char[4][b]: cliff tilesets IDs (cliff tilesets table)
      Example: "CLdi" stands for Lordaeron Cliff Dirt
      (refer to the files "TerrainArt\CliffTypes.slk" located in your war3.mpq for more details)
      the cliff tile list is actually ignored, the World Editor will simply add the cliff tiles for each tile in the ground tile list, if a cliff version of this ground tile exists
    - int: width of the map + 1 = Mx
    - int: height of the map + 1 = My
      Example: if your map is 160x128, Mx=A1h and My=81h
    - float: center offeset of the map X
    - float: center offeset of the map Y
      These 2 offsets are used in the scripts files, doodads and more.
      The orginal (0,0) coordinate is at the bottom left of the map (looking from the top) and it's easier to work with (0,0) in the middle of the map so usually, these offsets are :
      -1*(Mx-1)*128/2 and -1*(My-1)*128/2
      where:
      (Mx-1) and (My-1) are the width and the height of the map
      128 is supposed to be the size of tile on the map
      /2 because we don't want the length but the middle.
      -1* because we're "translating" the center of the map, not giving it's new coordinates
    
    Data:
    Each tilepoint is defined by a block of 7 bytes.
    The number of blocks is equal to Mx*My.
    - short: ground height
      C000h: minimum height (-16384)
      2000h: normal height (ground level 0)
      3FFFh: maximum height (+16383)
    - short: water level + map edge boundary flag*(see notes)
    - 4bit: flags*(see notes)
    - 4bit: ground texture type (grass, dirt, rocks,...)
    - 1byte: texture details 
      (of the tile of which the tilepoint is the bottom left corner) (rocks, holes, bones,...). It appears that only a part of this byte is used for details. It needs more investigations
    - 4bit: cliff texture type
    - 4bit: layer height
    

\*flags notes:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    Flags values (shown as big endian):
    0x4000 --> boundary flag 1 (shadow generated by the world editor on the edge of the map)
    0x0010 --> ramp flag (used to set a ramp between two layers)
    0x0020 --> blight flag (ground will look like Undead's ground)
    0x0040 --> water flag (enable water)
    0x0080 --> boundary flag 2 (used on "camera bounds" area. Usually set by the World Editor "boundary" tool.)
    
    Water level:
    Water level is stored just like ground height. The highest bit (bit 15) is used for the boundary flag 1.
    

Tilepoint data example:

    1
    51 21 00 62 56 84 13
    

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    51 21 --(little endian)--> 0x2151 --(hex-->dec)--> height = 8529
    00 62 --(little endian)--> 0x6200
    (extract boundary flag)--> (0x6200 & 0xC000) = 0x4000 boundary flag set
    (extract water data)--> (0x6200 & 0x3FFF) = 0x2200 --(hex-->dec)--> water level = 8704
    56 --> 5 sets both water flag and ramp flag, 6 means tilepoint is using ground type #6 in the tilesets table
    84 --> means tilepoint is using detail texture #132 (=0x084)
    13 --> 1 means cliff type #1 (cliff tilesets table), 3 means tilepoint on layer "3"
    The tilepoint "final height" you see on the WE is given by:
    (ground_height - 0x2000 + (layer - 2)*0x0200)/4
    where "0x2000" is the "ground zero" level, 2 the "layer zero" level and "0x0200" the layer height
    = (0x2151 - 0x2000 + 1*0x0200)/4
    = (8529 - 8192 + 512)/4
    = 212,25
    
    The tilepoint "water level" you see on the WE is given by:
    (water_level - 0x2000)/4 - 89.6
    where "0x2000" is the "ground zero" level, -89.6 is the water zero level (given by the water.slk file height = -0,7 --> water_zero_level = -0,7*128):
    = 8704/4 - 89,6
    = 38,4
    

In this case, water flag is set and water level is below the ground level so we will not see the water. This is just an example and I don’t think you can find such a tilepoint on a map. It was just here for demonstration purpose.

war3map.shd
===========

**The Shadow Map File**

This file has no header, only raw data.  
Size of the file = `16*map_width*map_height`  
1byte can have 2 values:  
`00h` = no shadow  
`FFh` = shadow  
Each byte set the shadow status of 1/16 of a tileset.  
It means that each tileset is divided in 16 parts (4\*4).

war3mapPath.tga/war3map.wpm
===========================

*   `war3mapPath.tga`: The Image Path file
*   `war3map.wpm`: The Path Map File

Only one of these two file is used for pathing. Old War3 beta versions (<=1.21) use the “war3mapPath.tga”.  
Since beta 1.30, Warcraft 3 uses a new file format instead: `"war3map.wpm"`.

war3mapPath.tga
---------------

**The Image Path file**

It’s an standard 32bits RGB TGA file with no compression and a black alpha channel. The TGA format is really important because if Warcraft III doesn’t recognise the file format, it’ll do weird things on the tilesets _(like put blight everywhere)_! Don’t forget the alpha channel! Each tile of the map is divided in 16 pixels _(4\*4 like in the shadow file)_, so the TGA width is `4*map_width` and its height is `4*map_height` and each pixel on the TGA affects a particular part of a tileset on the map. The color of a pixel sets the rules for that part. The top left corner of the image is the upper left corner on the map.

Header format (18 bytes):

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    byte: ID Length = 0
    byte: Color Map Type = 0
    byte: Image Type = 2 (uncompressed RGB)
    -- Color Map Specification (5 bytes) --
    byte[2]: First Entry Index = 0
    byte[2]: Color Map Length = 0
    byte: Color Map Entry Size = 0
    -- Image Spec (10 bytes) --
    byte[2]: X origin = 0
    byte[2]: Y origin = 0
    byte[2]: image width (little endian)
    byte[2]: image height (little endian)
    byte: Pixel depth = 32 (=0x20)
    byte: Image Descriptor = 0x28 (0x20=image starts from top left, 0x08=8bit for alpha chanel)
    

Example (where `"XX XX"` is a width and `"YY YY"` a height):

    1
    00 00 02 00 00 00 00 00 00 00 00 00 XX XX YY YY 20 28
    

Data:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    One pixel is defined by 4 bytes:
    BB GG RR AA
    Where:
    BB is the blue value (0 or 255)
    GG is the green value (0 or 255)
    RR is the red value (0 or 255)
    AA is the alpha chanel value (set to 0)
    There are 4*4 pixels for 1 tileset.
    The TGA width is map_width*4.
    The TGA height is map_height*4.
    

Here is the color code:

Color

Build state

Walk state

Fly state

White

no build

no walk

no fly

Red

build ok

no walk

fly ok

Yellow

build ok

no walk

no fly

Green

build ok

walk ok

no fly

Cyan

no build

walk ok

no fly

Blue

no build

walk ok

fly ok

Magenta

no build

no walk

fly ok

Black

build ok

fly ok

walk ok

To sum up, when red is set it means **“no walk”**, when green is set **“no fly”** and when blue is set **“no build”**.  
The alpha channel is used to set where blight is (black = normal, white = blight).

war3map.wpm
-----------

**The Path Map File**

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    Header:
    char[4]: file ID = 'MP3W'
    int: file version = 0
    int: path map width (=map_width*4)
    int: path map height (=map_height*4)
    
    Data:
    Each byte of the data part is a part of a tileset exactly like for the TGA.
    Data size: (map_height*4)*(map_with*4) bytes
    Flags table:
    0x01: 0 (unused)
    0x02: 1=no walk, 0=walk ok
    0x04: 1=no fly, 0=fly ok
    0x08: 1=no build, 0=build ok
    0x10: 0 (unused)
    0x20: 1=blight, 0=normal
    0x40: 1=no water, 0=water
    0x80: 1=unknown, 0=normal
    

Exmaple:

value

use

00

bridge doodad

08

shallow water

0A

deep water

40

normal ground

48

water ramp, unbuildable grounds, unbuildable parts of doodads

CA

cliff edges, solid parts of doodads (no build and no walk)

CE

edges of the map (boundaries)

war3map.doo
===========

**The doodad file for trees**

The file contains the trees definitions and positions.

“Frozen Throne expansion pack” format:

Header:

    1
    2
    3
    4
    char[4]: file ID = "W3do"
    int: file version = 8
    int: subversion? ([0B 00 00 00]h)
    int: number of trees defined
    

Data:  
Each tree is defined by a block of (usually) 50 bytes but in this version the length can vary because of the random item sets. The data is organized like this:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    char[4]: Tree ID
    int: Variation (little endian)
    float: Tree X coordinate on the map
    float: Tree Y coordinate on the map
    float: Tree Z coordinate on the map
    float: Tree angle (radian angle value)(degree = radian*180/pi)
    float: Tree X scale
    float: Tree Y scale
    float: Tree Z scale
    byte: Tree flags*
    byte: Tree life (integer stored in %, 100% is 0x64, 170% is 0xAA for example)
    int: Random item table pointer
      if -1 -> no item table
      if >= 0 -> items from the item table with this number (defined in the w3i) are dropped on death
    int: number "n" of item sets dropped on death (this can only be greater than 0 if the item table pointer was -1)
    then there is n times a item set structure
    int: Tree ID number in the World Editor (little endian) (each tree has a different one)
    

Tree ID can be found in the file `"doodads.slk"`.

\*flags:

    1
    2
    3
    0= invisible and non-solid tree
    1= visible but non-solid tree
    2= normal tree (visible and solid)
    

To sum up how it looks:

    1
    2
    3
    4
    5
    6
    7
    tt tt tt tt vv vv vv vv 
    xx xx xx xx yy yy yy yy 
    zz zz zz zz aa aa aa aa 
    xs xs xs xs ys ys ys ys 
    zs zs zs zs ff ll 
    bb bb bb bb cc cc cc cc 
    dd dd dd dd
    

where:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    tt: type
    vv: variation
    xx: x coordinate
    yy: y coordinate
    zz: z coordinates
    aa: rotation angle
    xs: x scale
    ys: y scale
    zs: z scale
    ff: flags
    ll: life
    bb: item table pointer
    cc: number of items dropped on death (0)
    dd: doodad number in the editor
    

After the last tree definition, there we have the special doodads (which can’t be edited once they are placed)

    1
    2
    3
    4
    5
    6
    7
    int: special doodad format version set to '0'
    int: number "s" of "special" doodads ("special" like cliffs,...)
    Then "s" times a special doodad structure (16 bytes each):
    char[4]: doodad ID
    int: Z? (0)
    int: X? (w3e coordinates)
    int: Y? (w3e coordinates)
    

Item set format (thx PitzerMike)

    1
    2
    3
    4
    5
    int: number "n" of items in the item set
    then we have n times then following two values (for each item set)
    char[4]: item id (as in ItemData.slk)
    this can also be a random item id (see bottom of war3mapUnits.doo definition)
    int: percentual chance of choice
    

war3mapUnits.doo
================

**The unit and item file**

The file contains the definitions and positions of all placed units and items of the map.  
Here is the Format:

NEW “Frozen Throne expansion pack beta” format (thx PitzerMike):

Header:

    1
    2
    3
    4
    char[4]: file ID = "W3do"
    int: file version = 8
    int: subversion? (often set to [0B 00 00 00]h)
    int: number of units and items defined
    

Data:  
Each unit/item is defined by a block of bytes (variable length) organized like this:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    41
    42
    43
    44
    45
    46
    47
    char[4]: type ID (iDNR = random item, uDNR = random unit) (thx PitzerMike)
    int: variation
    float: coordinate X
    float: coordinate Y
    float: coordinate Z
    float: rotation angle
    float: scale X
    float: scale Y
    float: scale Z
    char[4]: type ID (exists in new version)
    byte: flags*
    int: player number (owner) (player1 = 0, 16=neutral passive)
    byte: unknown (0)
    byte: unknown (0)
    int: hit points (-1 = use default)
    int: mana points (-1 = use default, 0 = unit doesn't have mana)
    int: map item table pointer (for dropped items on death)
    if -1 => no item table used
    if >= 0 => the item table with this number will be dropped on death
    int: number "s" of dropped item sets (can only be greater 0 if the item table pointer was -1)
    then we have s times a dropped item sets structures (see below)
    int: gold amount (default = 12500)
    float: target acquisition (-1 = normal, -2 = camp)
    int: hero level (set to1 for non hero units and items)
    int: strength of the hero (0 = use default)
    int: agility of the hero (0 = use default)
    int: intelligence of the hero (0 = use default)
    int: number "n" of items in the inventory
    then there is n times a inventory item structure (see below)
    int: number "n" of modified abilities for this unit
    then there is n times a ability modification structure (see below)
    int: random unit/item flag "r" (for uDNR units and iDNR items)
    0 = Any neutral passive building/item, in this case we have
      byte[3]: level of the random unit/item,-1 = any (this is actually interpreted as a 24-bit number)
      byte: item class of the random item, 0 = any, 1 = permanent ... (this is 0 for units)
      r is also 0 for non random units/items so we have these 4 bytes anyway (even if the id wasn't uDNR or iDNR)
    1 = random unit from random group (defined in the w3i), in this case we have
      int: unit group number (which group from the global table)
      int: position number (which column of this group)
      the column should of course have the item flag set (in the w3i) if this is a random item
    2 = random unit from custom table, in this case we have
      int: number "n" of different available units
      then we have n times a random unit structure
    
    int: custom color (-1 = none, 0 = red, 1=blue,...)
    int: Waygate: active destination number (-1 = deactivated, else it's the creation number of the target rect as in war3map.w3r)
    int: creation number
    

\*flags:

    1
    2
    3
    0= invisible and non-solid tree
    1= visible but non-solid tree
    2= normal tree (visible and solid)
    

Dropped item set format:

    1
    2
    3
    4
    5
    int: number "d" of dropable items
    "d" times dropable items structures:
    char[4]: item ID ([00 00 00 00]h = none)
    this can also be a random item id (see below)
    int: % chance to be dropped
    

**Inventory item format (thx PitzerMike)**

    1
    2
    3
    int: inventory slot (this is the actual slot - 1, so 1 => 0)
    char[4]: item id (as in ItemData.slk) 0x00000000 = none
    this can also be a random item id (see below)
    

**Ability modification format (thx PitzerMike)**

    1
    2
    3
    char[4]: ability id (as in AbilityData.slk)
    int: active for autocast abilities, 0 = no, 1 = active
    int: level for hero abilities
    

**Random unit format (thx PitzerMike)**

    1
    2
    3
    char[4]: unit id (as in UnitUI.slk)
    this can also be a random unit id (see below)
    int: percentual chance of choice
    

**Random item ids (thx PitzerMike)**  
random item ids are of the type `char[4]` where the 1st letter is `"Y"` and the 3rd letter is `"I"`  
the 2nd letter narrows it down to items of a certain item types

*   `"Y"` = any type
*   `"i"` to `"o"` = item of this type, the letters are in order of the item types in the dropdown box (`"i"` = charged)  
    the 4th letter narrows it down to items of a certain level
*   `"/"` = any level (ASCII 47)
*   `"0"` … = specific level _(this is ASCII 48 + level, so level 10 will be “:” and level 15 will be “?” and so on)_

**Random unit ids (thx PitzerMike)**  
random unit ids are of the type `char[4]` where the 1st three letters are “YYU”  
the 4th letter narrows it down to units of a certain level

*   `"/"` = any level (ASCII 47)
*   `"0"` … = specific level _(this is ASCII 48 + level, so level 10 will be “:” and level 15 will be “?” and so on)_

war3map.w3i
===========

**The info file**

It contains some of the info displayed when you start a game.

NEW Frozen Throne expansion pack format:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    41
    42
    43
    44
    45
    46
    47
    48
    49
    50
    51
    52
    53
    54
    55
    56
    57
    58
    59
    60
    61
    62
    63
    64
    65
    66
    67
    68
    69
    70
    int: file format version = 25
    int: number of saves (map version)
    int: editor version (little endian)
    String: map name
    String: map author
    String: map description
    String: players recommended
    float[8]: "Camera Bounds" as defined in the JASS file
    int[4]: camera bounds complements* (see note 1) (ints A, B, C and D)
    int: map playable area width E* (see note 1)
    int: map playable area height F* (see note 1)
      *note 1:
      map width = A + E + B
      map height = C + F + D
    int: flags
      0x0001: 1=hide minimap in preview screens
      0x0002: 1=modify ally priorities
      0x0004: 1=melee map
      0x0008: 1=playable map size was large and has never been reduced to medium (?)
      0x0010: 1=masked area are partially visible
      0x0020: 1=fixed player setting for custom forces
      0x0040: 1=use custom forces
      0x0080: 1=use custom techtree
      0x0100: 1=use custom abilities
      0x0200: 1=use custom upgrades
      0x0400: 1=map properties menu opened at least once since map creation (?)
      0x0800: 1=show water waves on cliff shores
      0x1000: 1=show water waves on rolling shores
      0x2000: 1=unknown
      0x4000: 1=unknown
      0x8000: 1=unknown
    char: map main ground type
      Example: 'A'= Ashenvale, 'X' = City Dalaran
    int: Loading screen background number which is its index in the preset list (-1 = none or custom imported file)
    String: path of custom loading screen model (empty string if none or preset)
    String: Map loading screen text
    String: Map loading screen title
    String: Map loading screen subtitle
    int: used game data set (index in the preset list, 0 = standard)
    String: Prologue screen path (usually empty)
    String: Prologue screen text (usually empty)
    String: Prologue screen title (usually empty)
    String: Prologue screen subtitle (usually empty)
    int: uses terrain fog (0 = not used, greater 0 = index of terrain fog style dropdown box)
    float: fog start z height
    float: fog end z height
    float: fog density
    byte: fog red value
    byte: fog green value
    byte: fog blue value
    byte: fog alpha value
    int: global weather id (0 = none, else it's set to the 4-letter-id of the desired weather found in TerrainArt\Weather.slk)
    String: custom sound environment (set to the desired sound lable)
    char: tileset id of the used custom light environment
    byte: custom water tinting red value
    byte: custom water tinting green value
    byte: custom water tinting blue value
    byte: custom water tinting alpha value
    int: max number "MAXPL" of players
    array of structures: then, there is MAXPL times a player data like described below.
    int: max number "MAXFC" of forces
    array of structures: then, there is MAXFC times a force data like described below.
    int: number "UCOUNT" of upgrade availability changes
    array of structures: then, there is UCOUNT times a upgrade availability change like described below.
    int: number "TCOUNT" of tech availability changes (units, items, abilities)
    array of structures: then, there is TCOUNT times a tech availability change like described below
    int: number "UTCOUNT" of random unit tables
    array of structures: then, there is UTCOUNT times a unit table like described below
    int: number "ITCOUNT" of random item tables
    array of structures: then, there is ITCOUNT times a item table like described below
    

**Players data format**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    int: internal player number
    int: player type
    1=Human, 2=Computer, 3=Neutral, 4=Rescuable
    int: player race
    1=Human, 2=Orc, 3=Undead, 4=Night Elf
    int: 00000001 = fixed start position
    String: Player name
    float: Starting coordinate X
    float: Starting coordinate Y
    int: ally low priorities flags (bit "x"=1 --> set for player "x")
    int: ally high priorities flags (bit "x"=1 --> set for player "x")
    

**Forces data format**: (thx Soar)

    1
    2
    3
    4
    5
    6
    7
    8
    int: Foces Flags
    0x00000001: allied (force 1)
    0x00000002: allied victory
    0x00000004: share vision
    0x00000010: share unit control
    0x00000020: share advanced unit control
    int: player masks (bit "x"=1 --> player "x" is in this force)
    String: Force name
    

**Upgrade availability change format** (thx PitzerMike)

    1
    2
    3
    4
    int: Player Flags (bit "x"=1 if this change applies for player "x")
    char[4]: upgrade id (as in UpgradeData.slk)
    int: Level of the upgrade for which the availability is changed (this is actually the level - 1, so 1 => 0)
    int Availability (0 = unavailable, 1 = available, 2 = researched)
    

**Tech availability change format** (thx PitzerMike)

    1
    2
    3
    int: Player Flags (bit "x"=1 if this change applies for player "x")
    char[4]: tech id (this can be an item, unit or ability)
    there's no need for an availability value, if a tech-id is in this list, it means that it's not available
    

**Random unit table format** (thx PitzerMike)

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    int: Number "n" of random groups
    then we have n times the following data (for each group)
    int: Group number
    string: Group name
    int: Number "m" of positions
    positions are the table columns where you can enter the unit/item ids, all units in the same line have the same chance, but belong to different "sets" of the random group, called positions here
    int[m]: for each positon is specified if it's a unit table (=0), a building table (=1) or an item table (=2)
    int: Number "i" of units/items, this is the number of lines in the table, each position can have that many or fewer entries
    now there's "i" times the following structure (for each line)
    int: Chance of the unit/item (percentage)
    char[m * 4]: for each position are the unit/item id's for this line specified
    this can also be a random unit/item ids (see bottom of war3mapUnits.doo definition)
    a unit/item id of 0x00000000 indicates that no unit/item is created
    

**Random item table format** (thx PitzerMike)

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    int: Number "n" of random item tables
    then we have n times the following data (for each item table)
    int: Table number
    string: Table name
    int: Number "m" of item sets on the current item table
    then we have m times the following data (for each item set)
    int: Number "i" of items on the current item set
    then we have i times the following two values (for each item)
    int: Percentual chance
    char[4]: Item id (as in ItemData.slk)
    this can also be a random item id (see bottom of war3mapUnits.doo definition)
    

war3map.wts
===========

**The trigger string data file**

Open it with notepad and you’ll figure out how it works. Each trigger string is defined by a number _(trigger ID)_ and a value for this number. When Warcraft meets a `"TRIGSTR_***"` _(where `"***"` is supposed to be a number)_, it will look in the trigger string table to find the corresponding string and replace the trigger string by that value. The value for a specific trigger ID is set only once by the first definition encountered for this ID: if you have two times the trigger string 0 defined, only the first one will count. The number following “STRING “ must be positive: any negative number will be ignored. If text follows “STRING “, it’ll be considered as number 0.

**String definition**:  
It always start with “STRING “ followed by the trigger string ID number which is supposed to be different for each trigger string. Then `"{"` indicates the begining of the string value followed by a string which can contain several lines and `"}"` that indicates the end of the trigger string definition.

Example:  
in the `.WTS` file you have:

    1
    2
    3
    4
    STRING 0
    {
    Blah blah blah...
    }
    

Then either in the `.J`, in the `.W3I` or in one of the object editor files, Warcraft finds a `TRIGSTR_000`, it’ll look in the table for  
trigger string number 0 and it’ll find that the value to use is “Blah blah blah” instead of `"TRIGSTR_000"`.  
If there are more than 999 strings another the reference simply becomes one character longer.

war3mapMap.blp
==============

the minimap image (with the help of BlacKDicK)

The BLP file contain the JPEG header and the JPEG raw data separated.  
BLP stands for `"Blip"` file which I guess is a `"BLIzzard Picture"`.  
There are two types of BLPs:

*   JPG-BLPs: use JPG compression
*   Paletted BLPs: use palettes and 1 or 2 bytes per pixel depending

The general format of JPG-BLPs:

Header:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    char[4]: file ID ("BLP1")
    int: 0 for JPG-BLP, 1 for Paletted
    int: 0x00000008 = has alpha, 0x00000000 = no alpha
    int: image width
    int: image height
    int: flag for alpha channel and team colors (usually 3, 4 or 5), 
      3 and 4 means color and alpha information for paletted files, 
      5 means only color information, if >=5 on 'unit' textures, it won't show the team color.
    int: always 0x00000001, if 0x00000000 the model that uses this texture will be messy.
    int[16]: mipmap offset (offset from the begining of the file)
    int[16]: mipmap size (size of mipmaps)
    

If it’s a JPG-BLP we go on with:

    1
    2
    3
    4
    int: jpg header size (header size) "h" (usually 0x00000270)
    byte[h]: header
    followed by 0 bytes until the begining of the jpeg data, we can safely erase these 0 bytes if we fix the mipmap offset specified above
    byte[16, mipmap size]: starting from each of the 16 mipmap offset addresses we read 'mipmap size' bytes raw jpeg data till the end of the file, having the header and the mipmap data we can process the picture like ordinary JPG files
    

If it’s a paletted BLP we go here:

    1
    2
    3
    4
    byte[4, 255]: the BGRA palette defining 256 colors by their BGRA values, each 1 byte
    byte[width x height]: the ColorIndex of each pixel from top left to bottom right, ColorIndex refers to the above defined color palette
    byte[width x height]: the AlphaIndex of each pixel on a standard greyscale palette for the alpha channel, where 0 is fully transparent and 255 is opaque,
    if the picturetype flag is set to 5, the image doesn't have an alpha channel, so this section will be omitted
    

More detailed blp specs by Magos: [http://magos.thejefffiles.com/War3ModelEditor/](http://magos.thejefffiles.com/War3ModelEditor/)

war3map.mmp
===========

**The menu minimap**

Header:

    1
    2
    int: unknown (usually 0, maybe the file format)
    int: number of datasets
    

Data:  
The size of a dataset is 16 bytes.

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    int: icon type
      Icons Types:
        00: gold mine
        01: house
        02: player start (cross)
    int: X coordinate of the icon on the map
    int: Y coordinate of the icon on the map
    Map Coordinates:
     top left: 10h, 10h
     center: 80h, 80h
     bottom right: F0h, F0h
    byte[4]: player icon color
    Player Colors (BB GG RR AA = blue, green, red, alpha channel):
     03 03 FF FF : red
     FF 42 00 FF : blue
     B9 E6 1C FF : cyan
     81 00 54 FF : purple
     00 FC FF FF : yellow
     0E 8A FE FF : orange
     00 C0 20 FF : green
     B0 5B E5 FF : pink
     97 96 95 FF : light gray
     F1 BF 7E FF : light blue
     46 62 10 FF : aqua
     04 2A 49 FF : brow
     FF FF FF FF : none
    

war3map.w3u / w3t / w3b / w3d / w3a / w3h / w3q
===============================================

**The custom units file**

Most of the W3U File specifications is from BlacKDicK.  
W3U files have a initial long and then comes two tables. Both look the same.  
First table is original units table (Original Blizzard Units).  
Second table is user-created units table (Units created by the map designer).

Header:

    1
    2
    3
    int: W3U Version = 1
    x bytes: Original Units table*
    y bytes: User-created units table*
    

Data:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    *Table definition:
    int: number n of units on this table.
    If 0 on original table, then skip default unit table. This is the number of following units. Even if we don't have any changes on original table, this value must be there.
    n times a unit definition structure*.
    
    *Unit definition structure:
    char[4]: original unit ID (get the IDs from "Units\UnitData.slk" of war3.mpq)
    char[4]: new unit ID. If it is on original table, this is 0, since it isn't used.
    int: number m of modifications for this unit
    m times a modification structure*
    
    *Modification structure:
    char[4] modification ID code (get the IDs from "Units\UnitMetaData.slk" of war3.mpq)
    int: variable type* t (0=int, 1=real, 2=unreal, 3=String,...)
    t type: value (length depends on the type t specified before)
    int: end of unit definition (usually 0)
    
    *Variable types:
    0=int
    1=real
    2=unreal
    3=string
    4=bool
    5=char
    6=unitList
    7=itemList
    8=regenType
    9=attackType
    10=weaponType
    11=targetType
    12=moveType
    13=defenseType
    14=pathingTexture
    15=upgradeList
    16=stringList
    17=abilityList
    18=heroAbilityList
    19=missileArt
    20=attributeType
    21=attackBits
    

NEW Frozen Throne expansion pack format of `"war3map.w3u / w3t / w3b / w3d / w3a / w3h / w3q"` The object data files (thanks PitzerMike):

These are the files that store the changes you make in the object editor.  
They all have one format in common. They have an initial int and then there are 2 tables  
Both look the same. The first table is the original table (Standard Objects by Blizzard).  
The second table contains the user created objects (Custom units / items / abilities …)

Header:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    int: File Version = (usually 1)
    x bytes: Original objects table*
    y bytes: Custom objects table*
    Data:
    *Table definition:
    int: number n of objects on this table, if 0 on the original table, then skip the default object table. This is the number of following units. Even if we don't have any changes on original table, this value must be there.
    n times a object definition structure*.
    *Object definition structure:
    char[4]: original object ID (see table at the bottom where you can get the object IDs*)
    char[4]: new object ID. (if it is on original table, this is 0, since it isn't used)
    int: number m of modifications for this object
    m times a modification structure*
    *Modification structure:
    char[4] modification ID (see the table at the bottom where you can get the mod IDs*)
    int: variable type* t
    [int: level/variation (this integer is only used by some object files depending on the object type, for example the units file doesn’t use this additional integer, but the ability file does, see the table at the bottom to see which object files use this int*) in the ability and upgrade file this is the level of the ability/upgrade, in the doodads file this is the variation, set to 0 if the object doesn't have more than one level/variation]
    [int: data pointer (again this int is only used by those object files that also use the level/variation int, see table*) in reality this is only used in the ability file for values that are originally stored in one of the Data columns in AbilityData.slk, this int tells the game to which of those columns the value resolves (0 = A, 1 = B, 2 = C, 3 = D, 4 = F, 5 = G, 6 = H), for example if the change applies to the column DataA3 the level int will be set to 3 and the data pointer to 0]
    int, float or string: value of the modification depending on the variable type specified by t
    int: end of modification structure (this is either 0, or equal to the original object ID or equal to the new object ID of the current object, when reading files you can use this to check if the format is correct, when writing a file you should use the new object ID of the current object here)
    

\*Variable types:

Value

Variable Type

Value Format

0

Integer

int

1

Real

float (single precision)

2

Unreal (0 <= val <= 1)

float (single Precision)

3

String

string (null terminated)

\*Object data files:

This table shows where to get the object IDs and the modification IDs and if the files use the 2 additional level and data pointer integers.

Extension

Object Type

Object IDs

Mod IDs

Uses Optional Ints

w3u

Units

Units\\UnitData.slk

Units\\UnitMetaData.slk

No

w3t

Items

Units\\ItemData.slk

Units\\UnitMetaData.slk

(those where useItem = 1) No

w3b

Destructables

Units\\DestructableData.slk

Units\\DestructableMetaData.slk

No

w3d

Doodads

Doodads\\Doodads.slk

Doodads\\DoodadMetaData.slk

Yes

w3a

Abilities

Units\\AbilityData.slk

Units\\AbilityMetaData.slk

Yes

w3h

Buffs

Units\\AbilityBuffData.slk

Units\\AbilityBuffMetaData.slk

No

w3q

Upgrades

Units\\UpgradeData.slk

Units\\UpgradeMetaData.slk

Yes

Note: These files can also be found in campaign archives with the exactly same format but named `war3campaign.w3u / w3t / w3b / w3d / w3a / w3h / w3q`

NEW Frozen Throne expansion pack format of `"*w3o"` object editor files (thanks PitzerMike):

The w3o is a collection of all above mentioned object editor files compiled in one single file. You get such a file if you export all object data in the object editor. It can be selected in the world editor as external data source in the map properties dialog, therefore it has to be in the same folder as the map that should use the file.

Format:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    int: file version (currently 1)
    int: contains unit data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3u file (see w3u specifications above)
    int: contains item data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3t file (see w3t specifications above)
    int: contains destructable data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3b file (see w3b specifications above)
    int: contains doodad data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3d file (see w3d specifications above)
    int: contains ability data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3a file (see w3a specifications above)
    int: contains buff data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3h file (see w3h specifications above)
    int: contains upgrade data file (1 = yes, 0 = no)
    if yes, then here follows a complete w3q file (see w3q specifications above)
    

war3map.wtg
===========

**The triggers names file**

Header:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    char[4]: file ID (WTG!)
    int: wtg file format version = 4
    int: number "a" of triggers categories
    "a" times a category definition structure*
    int: number "b" ???
    int: number "c" of variables
    "c" times a variable definition structure**
    int: number "d" of triggers
    "d" times a trigger definition structure***
    

**Category Definition Structure**:

    1
    2
    int: category index
    String: category name
    

**Variable Definition Structure**:

    1
    2
    3
    4
    5
    6
    String: variable name
    String: variable type
    int: number "e" ???
    int: array status: 0=not an array, 1=array
    int: initialisation status: 0=not initialized, 1=initialized
    String: initial value (string)
    

**Triggers Definiton Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    String: trigger name
    String: trigger description
    int: enable state: 0=disabled, 1=enabled
    int: custom text trigger state: 0=not a custom text trigger, 1=custom text trigger (use data in the WCT)
    int: initial state: 0=initially on, 1=not initially on
    int: ???
    int: index of the category the trigger belongs to
    int: number "i" of event-condition-action (ECA) function
    "i" times an ECA function definition structure*(4) (if it's a custom text trigger i should be 0 so we don't have this section)
    

**(4)ECA function definition structure**:

    1
    2
    3
    4
    int: function type: 0=event, 1=condition, 2=action
    String: function name
    int: enable state: 0=function disabled, 1=function enabled
    "x" times a parameter structure*(5). x depends of the function and is hardcoded.
    

**(5)Parameters Structure**:

    1
    2
    3
    4
    int: 0=preset, 1=variable, 2=function
    String: parameter value
    int: ???
    int: ???
    

**NEW Frozen Throne expansion pack format** (thanks Ziutek):

Header:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    char[4]: file ID (WTG!)
    int: wtg file format version = 7
    int: number "a" of triggers categories
    "a" times a category definition structure*
    int: number "b" ???
    int: number "c" of variables
    "c" times a variable definition structure**
    int: number "d" of triggers
    "d" times a trigger definition structure***
    

**Category Definition Structure**:

    1
    2
    3
    int: category index
    String: category name
    int: Category type: 0=normal, 1=comment
    

**Variable Definition Structure**:

    1
    2
    3
    4
    5
    6
    7
    String: variable name
    String: variable type
    int: number "e" ???
    int: array status: 0=not an array, 1=array
    int: array size
    int: initialisation status: 0=not initialized, 1=initialized
    String: initial value (string)
    

**Triggers Definiton Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    String: trigger name
    String: trigger description
    int: trigger type: 0=normal, 1=comment
    int: enable state: 0=disabled, 1=enabled
    int: custom text trigger state: 0=not a custom text trigger, 1=custom text trigger (use data in the WCT)
    int: initial state: 0=initially on, 1=not initially on
    int: run on map initialization: 0=no, 1=yes
    int: index of the category the trigger belongs to
    int: number "i" of event-condition-action (ECA) function
    "i" times an ECA function definition structure*(4) (if it's a custom text trigger it should be 0 so we don't have this section)
    

**(4)ECA function definition structure**:

    1
    2
    3
    4
    5
    6
    7
    int: function type: 0=event, 1=condition, 2=action
    String: function name
    int: enable state: 0=function disabled, 1=function enabled
    "x" times a parameter structure*(5). x depends of the function and is hardcoded.
    int: ???
    int: number "i" of event-condition-action (ECA) function
    "i" times an ECA function definition structure*(4)(if this trigger doesn't have multiple actions it should be set to 0 so we don't have this section)
    

**(5)Parameters Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    int: type which can be 0=preset, 1=variable, 2=function, 3=string
    String: parameter value
    int: begin function (if it is function it should be set to 1 otherwise to 0)
    if begin function is set to 1:
    int: type: 3
    String: the same as parameter value
    int: begin function: 1
    "x" times a parameters structure*(5). x depends on the function and is hardcoded.
    int: end function (always set to 0)
    

war3map.w3c
===========

**The camera file**

Header:

    1
    2
    int: file version = 0
    int: number "n" of camera definition structures
    

**Camera definition structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    float: Target X
    float: Target Y
    float: Z Offset
    float: Rotation (angle in degree)
    float: Angle Of Attack (AoA) (angle in degree)
    float: Distance
    float: Roll
    float: Field of View (FoV) (angle in degree)
    float: Far Clipping (FarZ)
    float: ??? (usually set to 100)
    String: Cinematic name
    

war3map.w3r
===========

**The triggers regions file**

Header:

    1
    2
    int: version = 5
    int: number "n" of region definition structures*
    

Data:

    1
    "n" times a region definition structure
    

**Region Definition Structures**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    float: left (jass coordinates)
    float: right (jass coordinates)
    float: bottom (jass coordinates)
    float: top (jass coordinates)
    String: region name
    int: Region index number (creation number)
    char[4]: weather effect ID (ex.: "RLlr" for "Rain Lordaeron Light Rain")
    If all the chars are set to 0, then weather effect is disabled.
    String: ambient sound (a sound ID name defined in the w3s file like "gg_snd_HumanGlueScreenLoop1")
    bytes[3]: region color (used by the World Editor) (BB GG RR)
    byte: end of the structure
    

war3map.w3s
===========

**The sounds definition file**

Header:

    1
    2
    int: file format version = 1
    int: number "n" of sounds defined
    

Data:

    1
    "n" times a sound definition structure*
    

**Sound definition structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    41
    42
    43
    44
    45
    46
    47
    String: sound ID name (like "gg_snd_HumanGlueScreenLoop1")
    String: sound file (like "Sound\Ambient\HumanGlueScreenLoop1.wav")
    String: EAX effects (like "DefaultEAXON")
    DefaultEAXON=Default
    CombatSoundsEAX=combat
    KotoDrumsEAX=drums
    SpellsEAX=spells
    MissilesEAX=missiles
    HeroAcksEAX=hero speech
    DoodadsEAX=doodads
    int: sound flags
    0x00000001=looping
    0x00000002=3D sound
    0x00000004=stop when out of range
    0x00000008=music
    0x00000010=
    int: fade in rate
    int: fade out rate
    int: volume (-1=use default value)
    float: pitch
    float: ???
    int: ??? (-1 or 8)
    int: channel:
    0=General
    1=Unit Selection
    2=Unit Acknowledgement
    3=Unit Movement
    4=Unit Ready
    5=Combat
    6=Error
    7=Music
    8=User Interface
    9=Looping Movement
    10=Looping Ambient
    11=Animations
    12=Constructions
    13=Birth
    14=Fire
    float: min. distance
    float: max. distance
    float: distance cutoff
    float: ???
    float: ???
    int: ??? (-1 or 127)
    float: ???
    float: ???
    float: ???
    

Note:

Floats value can be set or left unset _(default value will be used)_  
When a float is not set, the value `[4F800000]h` = 4.2949673e+009 is used.

war3map.wct
===========

**The custom text trigger file**

(with the help of BlackDick)

    1
    2
    3
    int: file version (0)
    int: number "n" of triggers
    "n" times a custom text trigger structure*
    

Note: Custom text trigger structures are defined using the same order as triggers they belong to in the WTG.  
Each trigger must have its custom text part, even if it has not been converted to a custom text trigger; in this case, the custom text size is set to 0 **(only 4 bytes for the size int)**.

**Custom Text Trigger Structure**:

    1
    2
    int: size "s" of the text (including the null terminating char)
    String: custom text trigger string (contains "s chars including the null terminating char)
    

NEW Frozen Throne expansion pack format (thanks Ziutek):

    1
    2
    3
    4
    5
    int: file version: 1
    String: custom script code comment
    1 time a custom text trigger structure*
    int: number "n" of triggers
    "n" times a custom text trigger structure*
    

Note: Custom text trigger structures are defined using the same order as triggers they belong to in the WTG.  
Each trigger must have its custom text part, even if it has not been converted to a custom text trigger; in this case, the custom text size is set to 0 (only 4 bytes for the size int).

\*Custom Text Trigger Structure:

    1
    2
    int: size "s" of the text (including the null terminating char)
    String: custom text trigger string (contains "s chars including the null terminating char)
    

war3map.imp
===========

**The imported file list**

Header:

    1
    2
    int: file format version
    int: number "n" of imported files
    

Data:

    1
    2
    1byte: tells if the path is complete or needs "war3mapImported\" (5 or 8= standard path, 10 or 13: custom path) (thx PitzerMike)
    String["n"]: the path inside the w3m of each imported file (like "war3mapImported\mysound.wav")
    

Note: Any file added in the `W3M` and added in the `.imp` will NOT be removed by the World Editor each time you save your map.  
This file can also be found in Warcraft campaign files with the name `war3campaign.imp` with the only difference that the standard path for imported files is `"war3campaignImported\"`.

war3map.wai
===========

**The Artificial Intelligence file**

File for the Frozen Throne expansion pack only (thanks to Ziutek)

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    31
    32
    33
    34
    35
    36
    37
    38
    39
    40
    41
    42
    43
    44
    45
    46
    47
    48
    49
    50
    51
    52
    53
    54
    55
    56
    57
    58
    59
    60
    61
    62
    63
    64
    65
    66
    67
    68
    69
    70
    71
    72
    73
    74
    75
    76
    77
    78
    79
    80
    int: wai file format version: 2
    string: AI name
    int: race: 0=Custom, 1=Human, 2=Orc, 3=Undead, 4=Night Elf
    int: options
    Unless otherwise noted all of the options are contained in HiWord of options integer.
    Check for the presence of options by using a mask with following values:
    
    SetPlayerName: 0x2000
    Melee: 0x0001
    DefendUsers: 0x8000
    RandomPaths: 0x4000
    TargetHeroes: 0x0002
    RepairStructures: 0x0004
    HeroesFlee: 0x0008
    UnitsFlee: 0x0010
    GroupsFlee: 0x0020
    HaveNoMercy: 0x0040
    IgnoreInjured: 0x0080
    RemoveInjuries: 0x1000
    TakeItems: 0x0100
    BuyItems: 0x0001 LoWord
    SlowHarvesting: 0x0200
    AllowHomeChanges: 0x0400
    SmartAltillery: 0x0800
    
    int: number of following workers and buildings. If there are less than 4 workers and buildings specified, WorldEdit will choose default values. Should be: 4.
    char[4]: gold worker
    char[4]: wood worker
    char[4]: base building
    char[4]: mine building (If none, use the same identifer as base building)
    
    int: number "a" of conditions
    int: ???: 7
    "a" times a conditions definition structure*
    char[4]: First Hero Used (if none set these values to null otherwise set it to Unit ID)
    char[4]: Second Hero Used (if none set these values to null otherwise set it to Unit ID)
    char[4]: Third Hero Used (if none set these values to null otherwise set it to Unit ID)
    int: Training Order % Chance(1.FirstHero,2.SecondHero,3.ThirdHero)
    int: Training Order % Chance(1.FirstHero,2.ThirdHero,3.SecondHero)
    int: Training Order % Chance(1.SecondHero,2.FirstHero,3.ThirdHero)
    int: Training Order % Chance(1.SecondHero,2.ThirdHero,3.FirstHero)
    int: Training Order % Chance(1.ThirdHero,2.FirstHero,3.SecondHero)
    int: Training Order % Chance(1.ThirdHero,2.SecondHero,3.FirstHero)
    
    Skill Selection Order:
    char[4*10]: Ten Skill IDs for First Hero(as first hero)
    char[4*10]: Ten Skill IDs for First Hero(as second hero)
    char[4*10]: Ten Skill IDs for First Hero(as third hero)
    char[4*10]: Ten Skill IDs for Second Hero(as first hero)
    char[4*10]: Ten Skill IDs for Second Hero(as second hero)
    char[4*10]: Ten Skill IDs for Second Hero(as third hero)
    char[4*10]: Ten Skill IDs for Third Hero(as first hero)
    char[4*10]: Ten Skill IDs for Third Hero(as second hero)
    char[4*10]: Ten Skill IDs for Third Hero(as third hero)
    int: number "c" of Build Priorities
    "c" times build priorities structure***
    int: number "d" of Harvest Priorities
    "d" times harvest priorities structure *(4)
    int: number "e" of Target Priorities
    "e" times target priorities structure *(5)
    int: repeats waves
    int: minimum forces: attack group index(for First Hero Only set this value to char[4]:HAIA)
    int: initial delay
    int: number "f" of Attack Groups
    "f" times attack groups structure *(6)
    int: number "g" of Attack Waves
    "g" times attack waves structure *(8)
    int: ???: 1
    int: game options
    Unless otherwise noted all of the options are contained in HiWord of options integer.
    Check for the presence of options by using a mask with following values:
    
    Disable Fog Of War: 0x0001
    Disable Victory/Defeat Conditions: 0x0002
    
    int: regular game speed
    string: path to map file
    int: number "h" of Players (0-2)
    "h" times players structure *(9)
    int: ???
    

**Condition Definition Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    int: condition index
    String: condition name
    int: number "b": 0=none(empty condition), 1=condition
    if number "b" = 1
    String: operator function name
    int: begin function: 1
    "x" times a Parameters Structure**. x depends on the function and is hardcoded.
    int: end function: 0
    

**Parameters Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    int: type: 0=preset, 1=operator function, 2=function, 3=string 
    String: parameter value (if it is function and this value is empty, begin function means '(' and end function means ')' )
    int: begin function (if it is function or operator function it should be set to 1 otherwise to 0)
    if it is function and parameter value is not empty:
    int: type: 3
    String: the same as parameter value
    int: begin function: 1
    "x" times a Parameters Structure**. x depends on the function and is hardcoded.
    int: end function: 0
    if it is operator function:
    int: begin function: 1
    "x" times a Parameters Structure**. x depends on the function and is hardcoded.
    int: end function
    int: end function: 0
    

**Build Priorities Structure**:

    1
    2
    3
    4
    5
    6
    int: priorities type: 0
    int: type: 0=unit, 1=upgrade, 2=expansion town
    char[4]: Unit/Upgrade ID(if it is expansion town this value should be set to: XEIA)
    int: town: 0=main, 1-9=Expansion #1-9, 0xFFFFFFFD-0xFFFFFFF5=current mine #1-9, 0xFFFFFFFF=any
    int: condition index(if none set this value to 0xFFFFFFFF, if custom set this value to 0xFFFFFFFE)
    1 time condition definition structure*(without condition index!)
    

**(4)Harvest Priorities Structure**:

    1
    2
    3
    4
    5
    6
    int: priorities type: 1
    int: harvest type: 0=gold, 1=lumber
    int: town: 0=main, 1-9=Expansion #1-9, 0xFFFFFFFD-0xFFFFFFF5=current mine #1-9, 0xFFFFFFFF=any
    int: workers: 0-90=fixed value #0-90, 0xFFFFFFFF=all, 0xFFFFFFFE=all not attacking
    int: condition index(if none, set this value to 0xFFFFFFFF, if custom, set this value to 0xFFFFFFFE)
    1 time condition definition structure*(without condition index!)
    

**(5)Target Priorities Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    int: priorities type: 2
    int: target: 0=common alliance target, 1=new expansion location, 2=enemy major assault,
    3=enemy expansion, 4=enemy any town, 5=creep camp, 6=purchase goblin zeppelin
    int: creep min strength(if target is other than 5 set this value to 0xFFFFFFFF)
    if target = 5
    int: creep max strength
    int: allow flyers: 0=no, 1=yes
    int: condition index(if none, set this value to 0xFFFFFFFF, if custom, set this value to 0xFFFFFFFE)
    1 time condition definition structure*(without condition index!)
    

**(6)Attack Groups Structure**:

    1
    2
    3
    4
    int: attack group index
    string: attack group name
    int: number "g" of Current Group
    "g" times current group structure *(7)
    

**(7)Current Group Structure**:

    1
    2
    3
    4
    5
    char[4]: Unit ID (FirstHero: 1HIA, SecondHero: 2HIA, ThirdHero: 3HIA)
    int: quantity value: 0-90=quantity #0-90, 0xFFFFFFFF=All
    int: maximum quantity: 0-90=quantity #0-90, 0xFFFFFFFF=All(if quantity value is 0xFFFFFFFF, maximum quantity means all except #0-90)
    int: condition index(if none, set this value to 0xFFFFFFFF, if custom, set this value to 0xFFFFFFFE)
    1 time condition definition structure*(without condition index!)
    

**(8)Attack Waves Structure**:

    1
    2
    int: attack group index
    int: delay
    

**(9)Players Structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    int: player index
    int: team number
    int: race: 1=human, 2=orc, 8=undead, 4=night elf, 20=random
    int: color: 0=red, 1=blue, 2=teal, 3=purple, 4=yellow, 5=orange, 6=green, 7=pink, 8=gray,
    9=light blue, 10=dark green, 11=brown
    int: handicap (0-100)
    int: ai: 0=standard, 1=user, 4=custom, 12=current
    int: ai difficulty: 0=easy, 1=normal, 2=insane
    string: path to custom ai script
    

war3mapMisc.txt, war3mapSkin.txt, war3mapExtra.txt
==================================================

**The Global Settings**

File for the Frozen Throne expansion pack only (thanks to PitzerMike)

These files hold information about global map settings stored in the format of ini files. These are plain textfiles where sections are initialized by `[sectionname]` and within each section values are set by a statement like valuename=value.  
The `war3mapMisc` file contains all the data of the gameplay constants screen..  
The `war3mapSkin` file contains all the changes made in the game interface screen.  
The `war3mapExtra` file contains the changes of the last tab in the map properties screen, where external data sources and custom skys are referenced.

W3N File Format
===============

(thanks to PitzerMike)

A `W3N` file is a Warcraft III Campaign file. It has the same 512 byte header as map files and also has a 260 byte footer for authentification purposes. See the `W3M` specification for details on the header and footer. These campaign files are only available in the Forzen Throne expansion pack.

The MPQ part can contain the following files:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    (listfile)
    (signature)
    (attributes)
    war3campaign.w3u
    war3campaign.w3t
    war3campaign.w3a
    war3campaign.w3b
    war3campaign.w3d
    war3campaign.w3q
    war3campaign.w3f
    war3campaign.imp
    war3campaignImported\*.*
    

We’ll see now what the war3campaign.w3f file stands for. The other files have already been discussed in the W3M specification.

war3campaign.w3f
================

(The info file for campaigns) (thanks to PitzerMike)

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    19
    20
    21
    22
    23
    24
    25
    26
    27
    28
    29
    30
    int: file format version (currently 1)
    int: campaign version (how many times it has been saved)
    int: saved with editor version
    String: campaign name
    String: campaign difficulty
    String: author name
    String: campaign description
    int: variable difficulty levels flag + expansion flag
    0=Fixed Diffculty, Only w3m maps
    1=Variable Difficulty, Only w3m maps
    2=Fixed Difficulty, Contains w3x maps
    3=Variable Difficulty, Contains w3x maps
    int: campaign background screen index (-1 = none or custom path)
    String: path of custom background screen (empty if none or preset)
    String: path of minimap picture (empty = none)
    int: ambient sound index (-1 = imported file, 0 = none, greater 0 = preset index)
    String: path of custom imported ambient sound mp3
    int: uses terrain fog (0 = not used, greater 0 = index of terrain fog style dropdown box)
    float: fog start z height
    float: fog end z height
    float: fog density
    byte: fog red value
    byte: fog green value
    byte: fog blue value
    byte: fog alpha value
    int: cursor and ui race index (0 = human)
    int: number "n" of maps in the campaign
    "n" times a map title structure*
    int: number "m" of maps in the flow chart (usually equal to "n")
    "m" times a map order structure*
    

**Map Title Structure**:

    1
    2
    3
    4
    int: is map visible in the campaign screen from the beginning (1 = visible, 0 = invisible)
    String: chapter title
    String: map title
    String: path of the map in the campaign archive
    

**Map Order Structure**:

    1
    2
    String: unknown (always empty, might also be a single character)
    String: path of the map in the campaign archive
    

SLK files
=========

SLK files can technically be opened by `Microsoft Excel`, though if you don’t own Excel or a SLK editor, or want to open SLK files with your application, you might be interested in the format, so here it is. SLK files are text files and read line by line. The first two letters of each line can be used to decide what to do with it.

Format:

*   Line `["ID"]`: the first line you will have to read is the one starting with ID, this id is usually set to `ID;PWXL;N;E`.
*   Line `["B;"]`: this line defines the number of columns and lines of the table.
    
    > Example: `B;Y837;X61;D0 0 836 60`  
    > This file has **837 lines** and **61 columns**, the rest of the line is always set to `;D0 0 Y-1 X-1` so these are probably the internal index bounds
    

From now on we will only look at lines that start with `"C;"` and are followed by `"Y"` or `"X"`.

*   Line `["C;Y"]`: specifies the value for the cell with the specified line and column if the fourth token starts with a `"K"`, otherwise we ignore it, anyway the current line is set to the y value, so the following lines that might be missing the y value refer to this line.
    
    > Example: `C;Y1;X1;K"unitBalanceID"`  
    > Sets the cell in line 1 and column 1 to “unitBalanceID”
    
*   Line `["C;X"]`: specifies the value for the cell with the specified column and the line previously specified as current line (in a `"C;Y"` statement) if the fourth token starts with a `"K"`, otherwise we ignore it.
    
    > Example: `C;X45;K1.8`  
    > Assigns 1.8 to the cell in current line and column 45  
    > The x value might also be omitted, so that the x value of the previous line is used.
    

All other lines can be ignored, they contain comments and format information. After a `"K"` there may either be a whole or floating point number or a string within quotes. Empty cells don’t have any entries.  
This should be enough information to successfully parse all of Blizzard’s SLK files and files edited with Excel or its Open Office equivalent. If you want to read up on the advanced tokens, get the official specs at [http://www.wotsit.org/download.asp?f=sylk](http://www.wotsit.org/download.asp?f=sylk)

BLP files
=========

(image format)

Besides TGA files, Warcraft III uses mainly the BLP format for images. The format supports JPG-compression and paletted images. See the `"war3mapMap.blp"` description for details on the BLP format. The TGA format is wide-spread and known by most graphic tools, anyway the TGA specs can be found at [http://www.wotsit.org/download.asp?f=tga](http://www.wotsit.org/download.asp?f=tga)

MDX and MDL files
=================

(model format)

The Warcraft III models are stored in MDX format.  
To convert an MDX file into readable MDL format and vice versa you can use Yobgul’s file converter, which is available at [http://www.wc3sear.ch/files/downloads.php?ID=3&l=6](http://www.wc3sear.ch/files/downloads.php?ID=3&l=6)

If Yobgul’s does’t work for you there’s an alternative programmed by Guessed available at [http://www.wc3campaigns.net/tools/weu/MDXReader1.00b.zip](http://www.wc3campaigns.net/tools/weu/MDXReader1.00b.zip)

There are several spec files on both formats available:

> [http://kmkdesign.8m.com/downloads/](http://kmkdesign.8m.com/downloads/) - Original specifications by KMK  
> [http://www.wc3campaigns.net/tools/specs/NubMdxFormat.txt](http://www.wc3campaigns.net/tools/specs/NubMdxFormat.txt) - More recent version by Nub  
> [http://magos.thejefffiles.com/War3ModelEditor/](http://magos.thejefffiles.com/War3ModelEditor/) - Latest specifications by Magos

Packed Files - W3V, W3Z, W3G
============================

(gamecache, savegame, replay files) (thanks to PitzerMike)

These files are packed using `zlib` compression. They have a common compressed format.  
But after unpacking the files, each type has its own file format. The packed format is as follows:

**Packed File Header**

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    15
    16
    17
    18
    char[28] signature: always "Warcraft III recorded game\0x1A\0"
    uint header size: 0x40 for versions up to 1.06, 0x44 for later versions
    uint compressed file size: including header
    uint header version: 0x00 up to version 1.06, 0x01 for later versions
    uint compressed size: size of the original decompressed file
    uint "n": nr of compressed data blocks in the file
    if (header version == 0) {
    ushort unknown: always 0
    ushort version number: the part behind the decimal point
    } else {
    char[4] version identifier: "WAR3" for classic, "W3XP" for expansion
    uint version number: the part behind the decimal point
    }
    ushort build number: that is a unique id for each Warcraft III build
    ushort flags: 0x0000 for single player, 0x8000 for multiplayer
    uint length: in milli seconds for replays, 0x00000000 otherwise
    uint checksum: CRC32 of the whole header until now, with this int set to 0
    now there are "n" compressed blocks
    

**Compressed Data Block**

ushort “m”: compressed size of the block  
ushort original size: size of the original uncomprssed input data,  
must be multiple of 2048, rest is filled with 0x00 bytes  
uint hash: checksum over the block header and the block data,  
the formula how this is computed won’t be released to the public,  
however reasonable requests might be answered

Now there are “m” bytes zlib compressed data, which can be decompressed/compressed with a zlib library.  
Depending on the zlib implementation you might have to compute the zlib header by yourself. That would be:

byte\[2\] zlib header: can be skipped when reading, set to 0x78 and 0x01 when writing  
byte\[m - 2\] deflate stream, use deflate stream implementation to decompress/compress

After uncompressing all blocks and appending them to each other, you have the original uncompressed file.  
Depending on the type of file, the replay, gamecache or savegame file specifications will now apply.

Campaigns.w3v
=============

(gamecache file) (thanks to PitzerMike)

This file is found once for each profile in the save directory of Warcraft III and contains stores variables and units  
carried from one map to another in campaigns. `Campaigns.w3v` files are also embedded in savegame files where they  
represent the in-memory state of the gamecaches at the time when the savegame was made. Here’s the format:

**Campaigns.w3v header**:

    1
    2
    3
    int: reserved for version (currently nr of gamecaches)
    int: nr "n" of gamecaches
    "n" gamecache structures
    

**Gamecache structure**:

    1
    2
    3
    4
    string: gamecache name
    int: reserved (currently nr of categories)
    int: nr "m" of categories
    "m" category structures
    

**Category structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    string: category name
    int[5]: reserved for additional variable types
    int: nr "i" integers
    "i" integer structures
    int: nr "r" reals
    "r" real structures
    int: nr "b" booleans
    "b" boolean structures
    int: nr "u" units
    "u" unit structures
    int: nr "s" strings
    "s" string structures
    

**Integer/Real/Boolean structure**:

    1
    2
    string: label name
    byte[4]: four bytes for the value
    

**String structure**:

    1
    2
    string: label name
    string: value
    

**Unit structure**:

    1
    2
    3
    4
    5
    6
    7
    8
    9
    10
    11
    12
    13
    14
    string: lable name
    int: unit id
    int: nr "j" inventory slots
    "j" inventory slot structures
    int: experience
    int: level
    int: unused skill points
    int[9]: unknown
    int: nr "k" hero skills
    "k" hero skill structures
    int[2]: unknown
    float: unknown
    int[4]: unknown
    short: unknown
    

**Inventory slot structure** (everything 0 if slot is empty):

    1
    2
    3
    int: item id
    int: nr charges
    int: unknown
    

**Hero skill structure** (everything 0 if not learned yet):

    1
    2
    int: ability id
    int: current level
    

W3Z files
=========

(savegames)

There’s not much to say about savegame files. Noone ever tried to track the format of Warcraft III savegames down  
because theyi are huge and the format is pretty complicated. The only information available here can be used to extract  
embedded gamecache data.  
To find the embedded gamecache data you simply have to search for the string token `Campaigns.w3v` in the savegame.  
After this token you will find a full `Campaigns.w3v` file as described in the above section. I never tried to reinject  
edited gamecaches into a savegame, but I’m pretty sure there’s a hash or checksum that would prevent it from working.

W3G files
=========

(replays) (thanks to Blue, Nagger and Kliegs)

The replay format has been tracked down by people at ShadowFlare’s realm. You can find very good and  
detailed documentation on the format at [http://w3g.deepnode.de/](http://w3g.deepnode.de/)

The others files
================

`war3mapImported\*.*` = files imported by the world editor or by the user using the import manager (these should be listed in the imp file, otherwise they will be deleted when the map is saved), you can now use any path you like and are not limited to `war3mapImported\` any more, this way you can override files in the Warcraft mpq archives by placing files with the same name in your map  
`(signature)`, `(listfile)` and `(attributes)` are rather extensions to the MPQ format than actual map files but they can also be found in map archives  
`(signature)` = signature of the `w3m` file, used only by Blizzard _(has been replaced by a stronger signature appended at the end of map archives)_  
`(listfile)` = list of files stored in the `w3m` file, used by MPQ editors to resolve the names of the files stored in the archive, this is needed because stored files are one-way-hashed  
`(attributes)` = attributes of files in the `w3m` file, it is checked when a map is loaded in the World Editor, if you have modified a file of a map and didn’t update the (attributes) file properly it will say that the map is corrupted, the easy way to prevent this is deleting the file from the archive, the harder way is updating it, the format of the file is as follows:

    1
    2
    3
    4
    5
    6
    7
    int: unknown1: set to 0x64000000
    int: unknown2: set to 0x03000000
    "n" times a CRC32, "n" is the number of used slots (=files) in the MPQ archive
    So for each file we have the CRC32 (cycling redundancy check) value here, wich is a sort of checksum, you can find functions that calculate the CRC32 of a file on the web
    "n" times a Filetime, "n" is the number of used slots (=files) in the MPQ archive
    The Filetime specifies for each file when it was added to the archive. It consists of two ints (low and high date/time). These two integers include information about the year, month, day, hour, minute and second when each file has been added, these 2 values represent a WinAPI file time structure and can be converted to the WinAPI system time type by using the SystemTimeToFileTime and FileTimeToSystemTime functions in kernel32.dllint: 
    int: low date time
    

The order of the CRC32 and Filetimes is the same as the order of the files in the archive. Logically this means that the information of the last CRC32 and Filetime is the information about the `(attributes)` file, because it has to be added to the archive after all other files. This also means that the CRC32 of the last file is always 0, because you cannot calculate the CRC32 of a file that will be changed later, which would make the CRC invalid again.  
Format Specification Backups:

Here are local copies of all the external format specifications that I have linked to in this file. Please always check out the external sites first, because there might be more up-to-date information. But just in case any of the link ever gets broken, here are local copies.

    1
    2
    3
    4
    5
    6
    7
    8
    9
    QuantamMPQFormat.txt
    KMKMdxFormat.txt
    MagosMdxFormat.txt
    NubMdxFormat.txt
    MagosBlpFormat.txt
    W3GFormat.txt
    W3GActions.txt
    SLKFormat.txt
    TGAFormat.txt
    

Credits:

Thanks to Justin Olbrantz (Quantam) for Inside MoPaQ, Andrey Lelikov for LMPQ API and ShadowFlare, they created the tools to allow me to get “inside” w3m (mpq) files.  
Special thanks to BlackDick, DJBnJack, PitzerMike, StonedStoopid, Ziutek and a few others who helped me to find some stuff and work on this documentation.  
Thanks to WC3Campaign Staff and War3Pub Staff who hosted my stuff and let me meet interesting people.  
Thanks to Blizzard Entertainment for making Warcraft III

const postNavMain = document.querySelector('.nav-main'); const toc = document.querySelector('.toc'); const tocParent = toc.parentElement; let tocToggled = false; function toggleTOC() { const matchMedia = window.matchMedia("(max-width: 800px)").matches; if (matchMedia != tocToggled) { if (matchMedia) { postNavMain.appendChild(toc); } else { tocParent.appendChild(toc); } tocToggled = matchMedia; } } function initEvent(type, name, obj) { obj = obj || window; var running = false; var func = function() { if (running) { return; } running = true; requestAnimationFrame(function() { obj.dispatchEvent(new CustomEvent(name)); running = false; }); }; obj.addEventListener(type, func); }; initEvent("resize", "optimizedResize"); window.addEventListener("optimizedResize", toggleTOC); toggleTOC(); function debounce(fn, delay) { var timer; return function() { var context = this; var args = arguments; clearTimeout(timer); timer = setTimeout(() => { fn.apply(context, args); }, delay); }; } function throttle(fn, delay) { var timer; var isFirst = true; return function() { var context = this; var args = arguments; if (isFirst) { fn.apply(context, args); isFirst = false; } if (timer) { return; } timer = setTimeout(() => { timer = null; fn.apply(context, args); }, delay); }; } var contentArea = document.querySelector(".article"); var headers = document.querySelectorAll( ".article .post h1,h2,h3,h4,h5,h6" ); var tocList = document.querySelectorAll(".toc a"); function highlightActiveToc() { var activeIndex = 0; for (let i = 0; i < headers.length; i++) { var offsetContainerTop = headers\[i\].offsetTop - contentArea.scrollTop >= 0; if (offsetContainerTop > 0) { activeIndex = i; break; } } tocList.forEach(function(tocItem) { if (tocItem.getAttribute("href").substr(1) === headers\[activeIndex\].id) { tocItem.classList.add("active"); tocItem.scrollIntoViewIfNeeded(); } else { tocItem.classList.remove("active"); } }); } highlightActiveToc(); contentArea.onscroll = throttle(highlightActiveToc, 15);

var mNav = document.evaluate( "//nav\[@class='nav'\]/div\[@class='nav-main-container'\]/div\[@class='nav-main'\]/div\[@class='nav-menu'\]/a\[text()='W3X\_Files\_Format'\]", document, null, XPathResult.FIRST\_ORDERED\_NODE\_TYPE, null ).singleNodeValue; if (mNav) { mNav.classList.add("active"); } const navToggle = document.querySelector('.nav-main-toggle'); const navMainContainer = document.querySelector('.nav-main-container'); const navMain = navMainContainer.querySelector('.nav-main'); navToggle.addEventListener('click', function(){ navMainContainer.classList.toggle('active'); }); navMainContainer.addEventListener('click', function(){ navMainContainer.classList.remove('active'); }); navMain.addEventListener('click', function(e){ e.stopPropagation() });