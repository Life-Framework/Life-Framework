# Guide: Creating a Debug World for Life Framework

This guide provides step-by-step instructions on how to set up a debug world in the Arma Reforger Workbench to test all features of the Life Framework (Everon Life).

## 1. Prerequisites

*   **Workbench**: Open the Arma Reforger Tools.
*   **Project**: Ensure `LifeFramework.gproj` is loaded.
*   **Dependencies**: Ensure **Enfusion Persistence Framework (EPF)** is loaded in the project.

## 2. World Setup

1.  **Create New World**:
    *   File > New World.
    *   Select `Subscene` (or `Generic World` if you want a blank slate).
    *   Save it as `Worlds/DebugWorld/DebugWorld.ent`.

2.  **Add Game Mode**:
    *   Open the **Resource Browser**.
    *   Navigate to `Prefabs/MP/Modes/Roleplay`.
    *   Drag and drop `GameMode_Roleplay.et` into the world.
    *   **Important**: Ensure this is the *only* GameMode entity in the world.

3.  **Configure Game Mode**:
    *   Select the `GameMode_Roleplay` entity in the **Hierarchy**.
    *   In the **Object Properties**, look for `EL_GameModeRoleplay` component.
    *   Ensure all Managers are present (JobManager, ATMManager, etc.). They are usually auto-configured.

## 3. Spawning System Setup

To allow players to spawn, you need to configure the specialized Roleplay spawning system.

1.  **Add Spawn Points**:
    *   In Resource Browser, find `EPF_SpawnPoint.et` (from EPF addon) or `SCR_SpawnPoint.et`.
    *   Drag and drop one or more into the world at your desired start location.
    *   give them a faction if necessary (though Roleplay modes often handle this dynamically).

2.  **Configure Respawn Component**:
    *   Select the `GameMode_Roleplay` entity.
    *   Find the `SCR_RespawnSystemComponent` (or `EL_RespawnSystemComponent` if customized).
    *   Existing setup usually points to `EL_SpawnLogic`.
    *   **CRITICAL**: You must configure the **Default Character Prefabs**.
        *   Expand `m_pSpawnLogic`.
        *   Look for `m_aDefaultCharacterPrefabs`.
        *   Add a path to a playable character, e.g., `{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et` or any specific Life Framework character prefab you have in `Prefabs/Characters`.
        *   If this is empty, you will spawn as a "Ghost" or stick in the map view.

## 4. Setting Up a Shop (Trader)

Shops allow players to buy items.

1.  **Place Trader Entity**:
    *   Navigate to `Prefabs/Trader`.
    *   Drag `UniversalTrader.et` into the world.

2.  **Configure Shop Inventory**:
    *   Select the Trader entity.
    *   In properties, find `EL_ShopComponent`.
    *   **Shop Name**: Give it a name (e.g., "General Store").
    *   **Shop Items**: Click the `+` to add items.
        *   `Item Prefab`: Select an item (e.g., `Prefabs/Items/Food/Apple.et`).
        *   `Price`: Set a cost (e.g., 10).
        *   `Buy/Sell Scalar`: Adjust if you want buying/selling prices to differ.

3.  **Result**: When you approach this NPC in-game and interact, the Shop Menu should open.

## 5. Setting Up Processing (Work)

Processing points convert raw resources (Inputs) into processed goods (Outputs).

1.  **Place Object**:
    *   Navigate to `Prefabs/Prop/Industrial` or use a generic table/machine from `ArmaReforger` assets.
    *   Example: Place a `Workbench_01.et`.

2.  **Add Processing Action**:
    *   In Object Properties, click **Add Component**.
    *   Choose `ActionsManagerComponent`. (If it already exists, skip).
    *   Expand `ActionsManagerComponent` > `Additional Actions`.
    *   Click `+` and choose `EL_ProcessAction`.

3.  **Configure Processing**:
    *   Expand the `EL_ProcessAction` inside the component.
    *   **Inputs**: Add an element.
        *   `Input Prefab`: e.g., `Apple.et`.
        *   `Input Amount`: 1.
    *   **Outputs**: Add an element.
        *   `Output Prefab`: e.g., `AppleJuice.et` (you might need to create this prefab or use another item).
        *   `Output Amount`: 1.
    *   **Force Drop Output**: Check this if you want items to fall on the floor instead of going into inventory.

## 6. Setting Up Banking (ATM)

1.  **Place ATM**:
    *   Find an ATM visual prefab (search "ATM").
    *   Drag it into the world.

2.  **Add Interaction**:
    *   Currently, the `EL_CharacterATMComponent` handles the logic on the player.
    *   To open it via the world entity, you likely need a `EL_OpenMenuAction` configured to open menu `ATM`.
    *   *Note*: If a dedicated `EL_ATMAction` doesn't exist in the dropdown, the interaction might be handled via a general "Open Menu" generic action or keybind (Home key?).

## 7. Testing

1.  **Save World**: Save as `DebugWorld.ent`.
2.  **Play**: Click the **Play** button in Workbench.
3.  **Verify**:
    *   **Spawn**: Do you get the Character Creation screen?
    *   **Economy**: Can you see your money in HUD? (If HUD is setup).
    *   **Shops**: Can you buy the Apple?
    *   **Processing**: Can you turn the Apple into Juice?

## 8. Common Issues & Fixes

*   **"No Default Prefabs Configured"**: You forgot step 3.2. Go back to GameMode > EL_SpawnLogic and add a character prefab.
*   **"Menu preset not found"**: Ensure `Configs/System/chimeraMenus.conf` is properly assigned in the Project Settings or that the GameMode is loading the correct config.
*   **"EPF Error"**: You are missing the EPF dependency. Add it to `LifeFramework.gproj`.
