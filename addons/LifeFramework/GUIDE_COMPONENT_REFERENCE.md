# Life Framework Component Reference

This document serves as a technical reference for the key components in the Life Framework. Use this when configuring entities in the Workbench.

## Core Components

### `EL_GameModeRoleplay`
*   **Type**: GameMode Component (inherits `SCR_BaseGameMode`).
*   **Location**: Attached to `GameMode_Roleplay` prefab.
*   **Purpose**: Central hub. Initializes all managers (`ATMManager`, `JobManager`, etc.).
*   **Configuration**: None visually exposed. Logic is internal.

### `EL_SpawnLogic`
*   **Type**: Spawn Logic (inherits `EPF_BaseSpawnLogic`).
*   **Location**: Inside `SCR_RespawnSystemComponent` on the GameMode.
*   **Purpose**: Handles player account loading and character spawning.
*   **Crucial Settings**:
    *   `m_aDefaultCharacterPrefabs`: **REQUIRED**. List of character prefabs to spawn new players as.
    *   `m_aDefaultCharacterItems`: Optional basic kit for new players.

## Feature Components

### `EL_ShopComponent`
*   **Type**: Entity Component.
*   **Usage**: Attach to an NPC or Object to turn it into a shop.
*   **Settings**:
    *   `m_sShopName`: Display name in the menu.
    *   `m_sShopDescription`: Subtitle/Description.
    *   `m_aShopItems`: List of items.
        *   `Item Prefab`: The entity to give.
        *   `Price`: Cost in currency.

### `EL_ProcessAction`
*   **Type**: User Action (Scripted).
*   **Usage**: Add to `ActionsManagerComponent` on a workbench/machine.
*   **Settings**:
    *   `m_aProcessingInputs`:
        *   `Prefab`: What player loses.
        *   `Amount`: How much.
    *   `m_aProcessingOutputs`:
        *   `Prefab`: What player gains.
        *   `Amount`: How much.
    *   `m_bForceDropOutput`: If true, items spawn on ground at `m_vDropOffset`.

### `EL_CharacterATMComponent`
*   **Type**: Character Component.
*   **Usage**: Attached to `Character_Base` (or equivalent).
*   **Purpose**: Holds the local player's bank data.
*   **Methods**: `OpenATMMenu()`.

### `EL_CharacterSurvivalComponent`
*   **Type**: Character Component.
*   **Usage**: Attached to `Character_Base`.
*   **Purpose**: Tracks Hunger/Thirst.
*   **Settings**: Likely contains decay rates (need to inspect source to confirm exact properties exposed).

## Managers (Singletons)

These are not attached to objects but run globally.

*   **`EL_ATMManager`**: Handles transactions.
*   **`EL_JobManager`**: Handles rewards for gathering/processing (passive).
*   **`EL_PlayerAccountManager`**: Handles loading/saving via EPF.

## UI Menus (IDs)

Registered in `Configs/System/chimeraMenus.conf`.

*   `ATM`
*   `Shop`
*   `CharacterCreation`
*   `Inventory` (likely standard Reforger, but framework has `SplitQuantityDialog`)
*   `SurvivalHUD` (HUD layout)
