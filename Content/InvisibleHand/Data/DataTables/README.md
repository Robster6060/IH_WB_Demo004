# DataTables — CSV import scaffold

**Location:** `Content/InvisibleHand/Data/DataTables/`  
**Schema reference:** `InvisibleHand_DataTable_Recommendations.md` §11a2–11c  
**Town grid prototype:** `../../TownGrid/InvisibleHand_TownGrid_Prototype_Path.md`  
**Build palette catalog:** `../../UI/BuildPalette/InvisibleHand_RightBuildPalette_Catalog.md`  
**UI architecture:** `../../UI/InvisibleHand_UI_Architecture.md`

| CSV | C++ row struct | Row ID column (`---`, col 1) | Import Key Field (editor) | Rows | Status |
|-----|----------------|-------------------------------|----------------------------|------|--------|
| `DT_BuildPaletteItem.csv` | `FIHBuildPaletteItemRow` | matches `itemID` (col 2) | **empty** | 5 G-tab grid templates | Scaffold — runtime CSV load + optional editor asset |
| `DT_TownGridTemplate.csv` | `FIHTownGridTemplateRow` | matches `templateID` (col 2) | **empty** | 5 templates (T1–T5) | Scaffold — runtime CSV load + optional editor asset |
| `DT_TownGridHarmonicBlocks.csv` | `FIHTownGridHarmonicBlockRow` | matches `presetRowID` (col 2) | **empty** | Header only (0 data rows) | Populate before T2 Harmonic prototype |

**Blueprint enums:** `IH_BuildPaletteTypes.h` — `EIHBuildPaletteTab`, `EIHBuildPaletteInteraction`, `EIHBuildPaletteLevel`, `EIHTownGridTemplate`, `EIHParcelZoneCode`, `EIHHarmonicCardinalAxis`.

---

## Editor manual import (preferred for cooked builds)

1. Compile the game module (`IH_P1C10_Azgaar`) so the row structs are registered.
2. In Content Browser, right-click the CSV → **Import** (or **Reimport** if already imported).
3. Create / assign a **DataTable** asset:
   - **Row Type:** pick the matching struct (`FIHBuildPaletteItemRow`, `FIHTownGridTemplateRow`, or `FIHTownGridHarmonicBlockRow`).
   - **Import Key Field:** leave **empty** (do **not** set `itemID`, `templateID`, or `presetRowID`). CSVs use UE’s `---` row-name column: column 1 header is `---`, column 2 is the duplicate key field (`itemID` / `templateID` / `presetRowID`).
4. Save assets under `Content/InvisibleHand/Data/DataTables/` using names that match the CSV stems (e.g. `DT_BuildPaletteItem`).

Enum columns in CSV must match UENUM entry names exactly (`Grid`, `GripTemplate`, `MainGame`, `Squared`, `CIV`, `SPD`, etc.).

### Troubleshooting import errors

| Error | Cause | Fix |
|-------|--------|-----|
| `Expected column 'templateID' not found in input.` | **Import Key Field** set to `templateID` while CSV uses `---` row-name format | Clear **Import Key Field**, delete broken asset, reimport |
| `Expected column 'itemID' not found in input.` | Same, for `DT_BuildPaletteItem` | Clear **Import Key Field**, reimport |
| `Expected column 'presetRowID' not found in input.` | Same, for `DT_TownGridHarmonicBlocks` | Clear **Import Key Field**, reimport |
| `Cannot find Property for column 'Name'` | CSV uses `Name` column but struct has no `Name` property | Use `---,keyField,...` format; clear Import Key Field |
| `Duplicate row name '…'.` | **Import Key Field** set to a data column shared by multiple rows | Clear **Import Key Field**; row names must come from column 1 (`---`) |
| Row Type dropdown empty | C++ module not compiled | Build `IH_P1C10_Azgaar`, restart editor |

**Clean reimport:** delete the broken `.uasset`, right-click the `.csv` → **Import** (fresh), not Reimport on a bad asset. Re-save CSV as UTF-8 **without BOM** if the first column is not recognized as `---`.

---

## Runtime CSV fallback

`UIHTownGridDataSubsystem` (auto-registered `UGameInstanceSubsystem`) loads on `GameInstance` init:

1. Tries `LoadObject` on `/Game/InvisibleHand/Data/DataTables/DT_*.DT_*` if a cooked `.uasset` exists.
2. Falls back to `Content/InvisibleHand/Data/DataTables/DT_*.csv` via `UDataTable::CreateTableFromCSVString` (supports `---` row-name header; BOM stripped before parse).

Access from C++ or Blueprint:

- `GetBuildPaletteItemTable()`
- `GetTownGridTemplateTable()`
- `GetTownGridHarmonicBlocksTable()`

Successful loads log row counts to `LogIH_P1C10_Azgaar` (expect **5 / 5 / 0** rows for palette / template / harmonic blocks); failures emit warnings.

---

## Build / compile

After adding or changing row structs, rebuild the **IH_P1C10_Azgaar** C++ module (Visual Studio / Rider, or `Build.bat` from the UE install). Hot-reload in editor is not sufficient for new `USTRUCT` / `UENUM` types.

**Note:** Empty optional CSV fields are left blank. `structureCategory` is stored as `FName` until `EIHStructureCategory` expands beyond `None`.
