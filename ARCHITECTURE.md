# GameplayResources: architecture

Orientation notes for working on this plugin (written for Claude Code sessions; useful to humans too).
Read this before changing anything under `Plugins/GameplayResources/`. Paths are relative to the plugin root.

## What it is

A game-instance-wide key/value store for designer-defined data ("resources"). Each resource is identified
by an `FName` key and holds a **property bag** (`FInstancedPropertyBag`, StructUtils) whose layout (the
"schema") is declared in a DataTable. Blueprint gets typed per-property pins through custom K2 nodes; C++
reads/writes bags directly and can subscribe to changes.

Modules:

| Module | Type | Contents |
|---|---|---|
| `GameplayResources` (`Source/GameplayResources`) | Runtime | data model, subsystem, BP thunks, change-listener async action |
| `GameplayResourcesEditor` (`Source/GameplayResourcesEditor`) | UncookedOnly | K2 nodes, Key dropdown pin factory, `FGameplayResource` details customization |

The editor module used to be called `GameplayResourceSubsystemEditor`; `Config/DefaultEngine.ini` (project)
has a `PackageRedirects` entry from `/Script/GameplayResourceSubsystemEditor` so old Blueprints still load.

## Data model

- **Schema source**: `UGameplayResourcesDeveloperSettings` (Project Settings, `Config=Game`) →
  `GlobalResourcesDataTable` (soft ptr, `LoadSynchronous` on every access) with row struct
  `FGameplayResourceEntry`:
  - `Resources` (`FInstancedPropertyBag`): the schema *and* authored default values.
  - `bSetAsDefault`: seed the runtime value at startup / on reset. Otherwise the key starts unset.
  - `Description` (editor only): shown as hover text on Key pins.
  - Row name = resource key. `GetKeys()` feeds every `GetOptions` dropdown.
- **Helpers** (static on the settings class): `FindResourceBag(Key)` (authored bag), `FindResourceSchema(Key)`
  (`UPropertyBag*`, the layout). A key is "known" iff `FindResourceSchema` is non-null.
- **`FGameplayResource`** (`GameplayResource.h`): a BP-friendly *copy* of a resource: `Key` + `Bag`.
  `SyncBagToSchema()` reshapes the bag to the key's schema; `ImportFromSubsystem` / `ExportToSubsystem` copy
  to/from the runtime store. The details customization (`GameplayResourceCustomization.cpp`) reshapes `Bag`
  when `Key` changes.
- **`FGlobalResourceReference`**: just a key with the dropdown; for UPROPERTYs that name a resource.

## Runtime store: `UGameplayResourceSubsystem` (GameInstanceSubsystem)

`TMap<FName, FInstancedPropertyBag> Values`. A key absent from the map is **unset**.

| Call | Effect | Broadcast |
|---|---|---|
| `Initialize` | seeds every `bSetAsDefault` row | none (no listeners can exist yet) |
| `SetResourceBag(Key, Bag)` | creates the bag from the schema if unset, then `CopyMatchingValuesByID` | `Added` if created, else `Changed` |
| `BPSetProperty` thunk | same creation rule, writes one property | `Added` if created, `Changed` if written |
| `ResetValue(Key)` | removes, then re-seeds the row default if `bSetAsDefault` | re-seeded: `Changed` (or `Added` if it was unset); not re-seeded: `Removed` with the old value; unset and no default: nothing |
| `ClearValue(Key)` | removes, never re-seeds | `Removed` with the old value, if it was set |
| `IsResourceEqual(FGameplayResource)` | compares to the runtime value | none |

Bag writes match properties **by ID** (`CopyMatchingValuesByID`), so a bag built against an older schema still
writes the properties that survived. `IsResourceEqual` uses the same ID matching (fast path: whole-struct
`Identical` when both bags share a `UPropertyBag`), but requires *exactly* the same property set, so a stale
layout compares unequal. Unset key → `false`.

`Changed` fires on every write, even when the value is identical. Deliberate (no compare cost on writes).

### Change listeners (native)

```cpp
enum class EGameplayResourceChangeType : uint8 { None, Added = 1, Changed = 2, Removed = 4, All = 7 };  // UENUM, Bitflags
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnGameplayResourceChanged, FName Key, EGameplayResourceChangeType, const FInstancedPropertyBag&);
using FGameplayResourceChangeListener = FOnGameplayResourceChanged::FDelegate;

static FDelegateHandle RegisterResourceChangeListener(WorldContext, FName Key /*None = all keys*/, Mask, Listener);
static void UnregisterResourceChangeListener(WorldContext, FDelegateHandle);
static void UnregisterAllResourceChangeListeners(WorldContext, UserObject);
// + non-static Add/Remove/RemoveAll... on the instance
```

Internals and the invariants they rely on:
- `FResourceChangeListeners` = one `FOnGameplayResourceChanged` per change type (`ByChangeType[3]`, indexed by
  bit position). A listener for several types is added to each list as a *copy of the same delegate*; delegate
  copies keep their `FDelegateHandle` (verified in `DelegateInstancesImpl.h`), so **one handle removes it from
  every list**. `Add` `check`s this.
- `AnyKeyListeners` (Key = `NAME_None`) is a plain member; per-key sets live in
  `TMap<FName, TSharedRef<FResourceChangeListeners>> KeyListeners`. Shared refs so a broadcast can hold the set
  alive and stable while a listener registers another key (map realloc) or unregisters itself (set pruned).
  Empty sets are pruned on `Remove*`.
- Registering for an unknown key → `ensure` + invalid handle. Empty mask → invalid handle.
- `BroadcastResourceChange` copies the bag into a **snapshot** before broadcasting (only if someone listens):
  the bag usually lives in `Values`, which a listener can reallocate by setting another key. Key-specific
  listeners fire before any-key listeners. The `Resource` reference is valid only during the call.
- Listener callbacks may freely set/reset/clear resources and (un)register listeners.

## Blueprint surface

### Wildcard property thunks (`CustomThunk`)

`UGameplayResourceSubsystem::BPGetProperty/BPSetProperty` (runtime store, by Key) and
`UGameplayResourceFunctionLibrary::BPGetResourceProperty/BPSetResourceProperty` (an `FGameplayResource`) read or
write **one named property** through a wildcard `Value` pin (`CustomStructureParam`). All are
`BlueprintInternalUseOnly`; only the K2 nodes call them. Non-obvious details, documented inline in the `exec*`
bodies:
- Set thunks step the `Value` term into **scratch storage** shaped like the schema property: an unconnected
  pin compiles to a literal opcode that writes through the address it's given (stepping with `nullptr`
  crashes).
- Values are copied by hand with `CopyCompleteValue`, not `FInstancedPropertyBag::SetValue` (that overload
  re-applies the property offset to the source pointer).
- `ArePropertiesLayoutCompatible` (`Private/GameplayResourceWildcardValue.h`) guards every copy: enum vs byte
  pins, containers recurse into element types.

### K2 nodes (`GameplayResourcesEditor`)

`UK2Node_ResourceAccessorBase` (`K2Node_GlobalResourceAccessor.h/.cpp`) owns the shared machinery: resolve a
schema key → generate **one pin per schema property** (`RebuildResourcePins`, via
`UE::StructUtils::GetPropertyDescAsPin`, `PC_Enum` → `PC_Byte`), resync on key change, rebuild before
relinking on reconstruction. `IsFixedPin` says which pins are *not* property pins; **subclasses that add fixed
pins must override it**. Subclasses:

| Node | Key from | Expands into |
|---|---|---|
| Get / Set Global Resource | Key dropdown pin (+ hidden-ish `self` = WorldContext) | chained `BPGetProperty`/`BPSetProperty`, + `HasGlobalResource` for `Found` |
| Get / Set Gameplay Resource | traced from the wired `FGameplayResource` *variable's default Key* (through knots) | chained `BPGet/SetResourceProperty` |
| **On Global Resource Changed** (`K2Node_OnGlobalResourceChanged`) | Key dropdown | see below |

The Key dropdown is `FGameplayResourceGraphPanelPinFactory`: any `PC_Name` pin named `Key` on a
`UK2Node_ResourceAccessorBase` gets an `SGraphPinDataTableRowName` over the settings DataTable.

**On Global Resource Changed**: event graphs only (`IsCompatibleWithGraph` → `GT_Ubergraph`).
Pins: exec in, WorldContext (`self`), `Key`, `ChangeMask` (int bitmask over `EGameplayResourceChangeType`,
default All) → `Then`, `Async Action` (for `Cancel`), `On Changed`, `ChangeType`, `Resource`
(`FGameplayResource` snapshot), one output per schema property.

Same pattern as `ObjectiveOrientedAI`'s `K2Node_AddObjectiveAndWait`: the runtime class is a
`UCancellableAsyncAction` marked `meta = (HasDedicatedAsyncNode)` (so the stock async node isn't offered in the
palette), and this node spawns a stock `UK2Node_AsyncAction` as an intermediate and wires it:

1. `UK2Node_AsyncAction` for `UAsyncAction_ListenForGlobalResourceChange::ListenForGlobalResourceChange(
   WorldContext, Key, ChangeMask, PreviousListener)`. The engine expands it into factory → bind
   `OnResourceChanged` to a generated custom event (data pins via temp variables) → `Activate()` → `Then`.
2. `PreviousListener` is an internal ubergraph temp variable (ubergraph locals persist across executions); after
   `Then`, the new action is assigned into it. The factory cancels the previous action, so re-running the node
   replaces the listener instead of stacking a second one.
3. The async node's `OnResourceChanged` exec → one `BPGetResourceProperty(Resource, PropertyName)` per
   **connected** property pin → `On Changed`.

`UAsyncAction_ListenForGlobalResourceChange` (`Public/Async/`): `Activate` registers the native listener;
`HandleResourceChanged` wraps the bag into an `FGameplayResource` and broadcasts the dynamic delegate.
Lifetime, and why it is the way it is:
- Ubergraph frame references are **weak** unless the object has `RF_StrongRefOnFrame`
  (`FPersistentFrameCollector` in `BlueprintGeneratedClass.cpp`). `UBlueprintAsyncActionBase` sets that flag, so
  the Blueprint instance that ran the node keeps the action alive. A plain `UObject` proxy would be collected at
  the next GC. That was an actual bug in the first version of this node.
- The action is deliberately **not** `RegisterWithGameInstance`'d: that would keep it (and the listener) alive
  after its Blueprint owner dies. Since `UCancellableAsyncAction::ShouldBroadcastDelegates` defaults to
  "registered with a game instance", it is overridden to "has a listener handle".
- `Cancel()` unregisters, clears the BP bindings, and `SetReadyToDestroy()` drops `RF_StrongRefOnFrame`.
  `BeginDestroy` unregisters too (owner collected). No subsystem (e.g. editor preview) → never fires.

## Gotchas

- **Pin-name collisions**: property pins use the raw property names. A schema property called `Key`, `Found`,
  `Resource`, `ChangeMask`, `ChangeType`, `OnChanged` or `AsyncTaskProxy` collides with a fixed pin. Not guarded.
- A **linked** (non-literal) Key pin means the schema is unknown at edit time: no property pins, only
  `Resource`/`Found`.
- Every settings helper calls `LoadSynchronous` on the DataTable; cheap after the first load, but avoid it in
  hot loops on cold start.
- `ResetGlobalResource` uses `WorldContext` meta (was misspelled `WorldContex` until 2026-10), so that pin is
  hidden/auto-filled.
- `Changed` is not de-duplicated (see above). `IsResourceEqual` is available if a caller needs to filter.

## Repository / build notes

- This folder is tracked by **two** git repos: the studio project repo (owner, internal GitLab) and a nested
  personal repo (`.git` here, `github.com/MrCrackspider/GameplayResourceSubsystem`). Only the user's own code
  may go to the personal repo; never push studio history there. Ask before touching the nested repo.
- The nested repo's `.gitignore` (applies to the project repo too) whitelists by extension: only `.h/.cpp/
  .cs/.uplugin/.md/...` are visible. New file types (e.g. `.ini`, `.png`) need a `!*.ext` line or `git add -f`.
- Adding a UCLASS/USTRUCT/UENUM or changing reflected headers needs a full editor build (editor closed); with
  the editor open use `-SingleFile` compile checks (see project `CLAUDE.md`).
- Renaming reflected types or modules: add `[CoreRedirects]` to the project's `Config/DefaultEngine.ini`.
