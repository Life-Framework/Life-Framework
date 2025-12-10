# Deep Dive: Debug World Script Implementation Guide

This document provides granular documentation for every script you might need to implement when building a Debug World for the Life Framework. It details exactly what each script does, where to attach it, and how to configure every property.

---

## 1. Game Mode & Spawning
**Script**: `EL_SpawnLogic`  
**Location**: Attached to `GameMode_Roleplay` prefab (specifically inside `SCR_RespawnSystemComponent`).

This is the **most critical** script. If this is wrong, you cannot spawn.

### Properties
*   **`m_aDefaultCharacterPrefabs`** (Array of Resources)
    *   **Description**: A list of character prefabs (e.g. `.et` files) that a new player will spawn as.
    *   **Configuration**: Must have at least one entry.
    *   **Example**: `{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et`
*   **`m_aDefaultCharacterItems`** (Array)
    *   **Description**: Items to give the player on spawn.
    *   **Configuration**: Useful for testing specific tools without buying them.

---

## 2. Economy: Shops (Traders)
**Script**: `EL_ShopComponent`  
**Location**: Attached to any Entity (NPC, Table, Cash Register) to make it a shop.

### Properties
*   **`m_sShopName`** (String)
    *   **Description**: The title displayed at the top of the Shop Menu.
    *   **Example**: "General Store" or "Apple Trader".
*   **`m_sShopDescription`** (String)
    *   **Description**: Subtitle text.
*   **`m_aShopItems`** (Array)
    *   **Description**: The inventory of the shop.
    *   **Inner Properties**:
        *   **`Item Prefab`**: The `.et` file of the item (e.g. `Apple.et`).
        *   **`Price`**: (Integer) Base cost to buy.
        *   **`Sell Scalar`**: (Float, default 1.0) Multiplier for selling back. If 0.5, you sell it for 50% of the buy price.
        *   **`Buy Scalar`**: (Float, default 1.0) Multiplier for buying.
        *   **`Unlimited Stock`**: (Bool) If true, shop never runs out.

---

## 3. Jobs: Processing Objects
**Script**: `EL_ProcessAction`  
**Location**: Attached to `ActionsManagerComponent` on a static object (Table, Machine).

This script handles the "Transformation" of items (e.g. Copper Ore -> Copper Ingot).

### Properties
*   **`m_aProcessingInputs`** (Array)
    *   **Description**: What the player must HAVE to perform the action.
    *   **Inner Properties**:
        *   **`Prefab`**: The item classname/resource.
        *   **`Amount`**: How many are removed from inventory.
*   **`m_aProcessingOutputs`** (Array)
    *   **Description**: What the player GETS.
    *   **Inner Properties**:
        *   **`Prefab`**: The item classname/resource.
        *   **`Amount`**: How many are added.
*   **`m_bForceDropOutput`** (Bool)
    *   **Description**: Critical for "Physical" jobs.
    *   **True**: Items spawn on the ground (physics enabled).
    *   **False**: Items go directly into player inventory.
*   **`m_vDropOffset`** (Vector <X, Y, Z>)
    *   **Description**: Where the output spawns relative to the machine.
    *   **Example**: `0 1 0` (1 meter above the object).

---

## 4. Jobs: Gathering Resources
**Script**: `EL_GatherAction`  
**Location**: Attached to `ActionsManagerComponent` on a resource object (Tree, Rock, Bush).

### Properties
*   **`m_GatherItemPrefab`** (Resource)
    *   **Description**: The item given to the player (e.g., `Apple.et`).
*   **`m_GatherAmount`** (Integer)
    *   **Description**: Quantity per action.
*   **`m_GatherToolRequirement`** (Resource)
    *   **Description**: Optional tool needed (e.g., `Pickaxe.et`).
    *   **Note**: Leave empty for hand-gathering.
*   **`m_CheckInventoryForToolRequirement`** (Bool)
    *   **Description**:
        *   **True**: Tool just needs to be in backpack.
        *   **False**: Tool must be in **Hands**.
*   **`m_GatherAmountMax`** (Integer)
    *   **Description**: Total times this specific object can be harvested before "depleting".
*   **`m_GatherTimeout`** (Float)
    *   **Description**: Time in **Milliseconds** (ms) for the resource to respawn.
    *   **Example**: `60000` = 1 minute.

---

## 5. Interactions: Teleporting/Markers
**Script**: `EL_TeleportAction` (Theoretical/Common Mod Pattern)
*Note: If this script exists in `Scripts/Game/Feature/Map` or similar.*

If not present, you normally use standard Reforger `SCR_TeleportUserAction`.

---

## 6. Banking: ATM Interaction
**Script**: `EL_OpenMenuAction` (Generic)  
**Location**: Attached to ATM Object.

Since specific `EL_ATMAction` might not be exposed, you often use a generic menu opener.

### Properties
*   **`m_MenuPreset`**: Set this to "ATM".

---

## 7. Police: Confiscating
**Script**: `EL_ConfiscateAction`  
**Location**: Likely added dynamically to players via `EL_PoliceManager`, but can be tested by attaching to a test dummy entity.

*   **Description**: Removes illegal items from target inventory.

---

## How to use this Guide
1.  **Open Workbench**.
2.  **Create an Entity** (GenericEntity or specific Prop).
3.  **Add Component** -> `ActionsManagerComponent`.
4.  **Add Action** -> Select `EL_GatherAction` (for example).
5.  **Refer to this map** to know exactly what `m_GatherTimeout` means (remember it is ms!).
