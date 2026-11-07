# Text

`text/strings.yaml` replaces texts of the game. Most texts of the game's
text files have an ID, and the file gives a new text for an ID. The
original files don't change.

```yaml
strings:
  STR_STATISTICS: 'Pilot Records'
  MSG_LASERS_ARMED: '\03Primary cannons armed'
  SPEC_1_DESC: 'Space superiority fighter, refitted for the Alliance.'
```

## strings.yaml

The file has one section, `strings`, which maps IDs to texts.

- **IDs** can be written in upper or lower case. An ID can't be empty or
  have a `!`.
- **Texts** replace the whole text of the ID, exactly as the game's file
  writes it after the ID.
- Put every text in single quotes. A value that YAML doesn't read as text,
  such as `5` or `true` without quotes, is logged and ignored.
- When an ID appears twice, the game logs it and uses the last text.
- An ID that the game never asks for does nothing.

The game reads the file once, at startup. Restart OpenXWA after a change.
Without the file, the game shows its own texts. When the file isn't valid
YAML, the game logs an error and ignores it.

### Escapes and codes

Some texts of the game hold codes that the game reads. Copy them into the
new text as they are:

- In-flight messages start with a code such as `\03`, written with a
  backslash. In single quotes, YAML keeps the backslash, so
  `'\03Laser cannons armed'` reaches the game as the file writes it. Don't
  use double quotes, because YAML reads `\0` in double quotes as an escape
  of its own.
- The long craft names of `STRINGS.TXT`, whose IDs start with `KSPEC`,
  start with their grammatical gender, `m:`, `f:` or `n:`, such as
  `!KSPECXW!m:X-wing`. Keep it, because without it the game can't load
  its texts.
- In single quotes, write a quote as two quotes: `'Rebel''s base'`.

### Characters

The file is UTF-8, like every YAML file of a mod. The game itself uses
Latin-1, so a text can only have the characters of Latin-1: the ASCII
characters and the accented letters and signs from U+00A0 to U+00FF. A
text with any other character, such as a curly quote `’` or a long dash
`—`, is logged and ignored. Use straight quotes `'` and `"` and a hyphen
`-` instead.

### Length

Some places in the game keep only a fixed number of characters and cut a
longer text. For example, the tech room keeps 63 characters for the name,
manufacturer, side and crew of a craft, and 255 for its description. Check
a long text in the game.

## Finding an ID

Find the text that you want to change in one of the game's text files, and
use the ID on the same line. A line with an ID looks like this:

```text
!STR_STATISTICS!Pilot Statistics
```

The ID is `STR_STATISTICS`, and the text is `Pilot Statistics`. These files
of the game folder have IDs that the game reads:

| File | What it holds |
|-|-|
| `STRINGS.TXT` | texts in flight: in-flight messages, HUD texts, craft names, system names and error messages |
| `FRONTTXT.TXT` | texts of the menus and the concourse |
| `SHIPLIST.TXT` | the ship list: one line for each craft type, `SPECIES_<number>` |
| `SPECDESC.TXT` | the tech room descriptions: `SPEC_<number>_NAME`, `_MANU`, `_SIDE`, `_DESC` and `_CREW` |
| `JOYSTICK.TXT` | the functions that a joystick button can have |
| `MISSIONS/MISSION.LST`, `MELEE/MISSION.LST`, `COMBAT/MISSION.LST` | the names of battles and missions |
| `MISSIONS/EMAIL.TXT` | the e-mails in the concourse |
| `FRONTRES/CUTSCENE/CUTSCENE.TXT` | the descriptions of the cutscenes |
| `FRONTRES/MEDALS/FAMILYAWARDS.TXT` | the memento texts |

The game folder's files are in Windows-1252. Open them with an editor that
reads this encoding.

In `SHIPLIST.TXT` and `SPECDESC.TXT`, the number is the craft type number.
See [Craft types](reference/craft_types.md).

Use the ID on the line, even when it doesn't seem to match the text. Some
IDs of `STRINGS.TXT` describe another text than the one they hold. For
example, `KAUTOEJECTSYS` holds "Engines".

### Texts with a format

Some texts are records that the game reads further. A new text must keep
the format of the original:

- A `SPECIES_<number>` text of `SHIPLIST.TXT` is the whole ship list line:
  the name, the category, the flags and the icon numbers. To change only
  the name, use `ship_list.name` in a craft type file. See
  [Craft](craft.md).
- A `KEY_<name>` text of `JOYSTICK.TXT` starts with the key's code and the
  key, such as `97 A Target attacker of target`. Change only the
  description after them.

### Texts that strings.yaml can't change

- Lines without an ID, such as the lines of `CREDITS.TXT`.
- The texts inside a mission's `.TIE` file, such as the briefing, the
  in-flight messages and the goals, except the debriefing texts below.
  Change them in the `.TIE` file. See [Missions](missions.md#tie-missions).

### Debriefing texts

The debriefing of a Tour of Duty mission shows texts of its `.TIE` file:
lines for a success, lines for a failure, and a hint after a failure. Each
of them has an ID built from the mission's file name, which
`strings.yaml` can use. The numbers after `B` and `M` in the file name
are `<b>` and `<m>`. For example, `1B7M2W.TIE` has `<b>` 7 and `<m>` 2:

| Text | ID | Example |
|-|-|-|
| Success line n | `W0<b>0<m>0<n>` | `W070201` for the first line |
| Failure line n | `L0<b>0<m>0<n>` | `L070201` |
| The hint after a failure | `<mission>_L00`, with the file name without its extension | `1B7M2W_L00` |

```yaml
strings:
  W070201: 'The convoy is safe.'
```

## Other files that set texts

These mod files also set texts. Where they set the same text as
`strings.yaml`, they win:

| File | Texts |
|-|-|
| a craft type file, `ship_list` and `tech_room` | the ship list line and the tech room texts of the craft type |
| an object type file, `names` | the names of the craft in flight |
| a mission file, `craft_names` | the names of a model's craft, for one mission |

[Craft](craft.md) describes the craft type and object type files.
[Missions](missions.md#craft_names) describes `craft_names`.

A copy of a text list in `game/`, such as `game/MISSIONS/EMAIL.TXT`, can
also change texts. The `!ID!` lines of the copy still go through
`strings.yaml`. See [Missions](missions.md#e-mails-and-other-text-lists).
