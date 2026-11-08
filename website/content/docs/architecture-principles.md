---
title: "Architecture & Principles"
slug: architecture-principles
order: 1
created: 2025-11-30
updated: 2025-11-30
source: lovable-export
---
Welcome to the development documentation for our Life Mod. Our goal is to build a framework that is **flexible**, **easy to configure**, and **friendly to modders**. If you are new to Enfusion, these guidelines will help you understand how we build things here.

* * *

## 1\. Declarative Configuration (The "Settings" Rule)

**Concept:** Imagine you are a server owner who doesn't know how to code. You want to change the price of an Apple from $5 to $10. You shouldn't have to open a script file to do that.

**The Rule:** Never "hard-code" numbers (prices, timers, limits) inside your scripts. Instead, we use **Config Files**.

**How it works in Enfusion:** We use a special tag called `[Attribute]`. When you put this above a variable in your code, it magically appears in the **Workbench Property Grid**.

**Example:**

❌ **Bad (Hardcoded):**

```
// Inside a script...
int applePrice = 5; // To change this, you must recompile the mod!
```

✅ **Good (Declarative):**

```
[BaseContainerProps(configRoot: true)]
class Life_EconomyConfig : ScriptObject
{
    [Attribute("5", UIWidgets.EditBox, "Price of an apple")]
    int m_iApplePrice;
}
```

_Now, a server owner can just open the Config File in the editor and type "10"._

* * *

## 2\. Client vs. Server (The "Controller" Approach)

**Concept:** In multiplayer, the Client (the player's PC) and the Server (the host) are two different worlds.

- **The Client** captures button presses (UI).
- **The Server** makes the decisions (changing bank balance).

**The Architecture:** We do not use the standard `SCR_PlayerController` because it is messy to overwrite vanilla files. Instead, we create a custom **Network Entity** called `LifeSessionController`.

**How it flows:**

1. Player joins -> Server spawns a `LifeSessionController` just for them.
2. Player clicks "Buy Apple" -> The Client sends a message (**RPC**) to their Controller.
3. The Server receives the message -> Checks if they have money -> Deducts money -> Tells Client "Success".

> **Beginner Tip:** Think of the `LifeSessionController` as the player's personal "Assistant." The player tells the Assistant what they want, and the Assistant talks to the Server Manager to get it done.

* * *

## 3\. Persistence (Saving Data)

**Concept:** We need to save the player's money and items when they leave. We use the **native Reforger save system** (built by the developer Arkensor).

**The Rule:** Do not write your own file handling scripts. We use "Structs" (data containers).

**How to use it:**

1. Define a class that holds data (e.g., `class Life_SaveData`).
2. Mark variables you want to save using the `RegV` (Register Variable) command.
3. The game engine handles writing this to a database (JSON) automatically.

* * *

## 4\. User Interface (UI)

**Concept:** UI in Enfusion consists of two parts:

1. **The** `.layout` **file:** This is the visual part (images, text boxes) created in the Layout Editor.
2. **The** `.c` **script:** This is the logic that controls the layout.

**The Rule:** Don't use "OnFrame" (updating 60 times a second) to check if money changed. That slows the game down. Instead, make the UI **Reactive**.

**The Reactive Workflow:**

1. Something changes (Money goes up).
2. The system shouts, "Money Changed!" (This is called an **Event** or **Invoker**).
3. The UI is listening. It hears the shout and _only then_ updates the text on the screen.

* * *

## 5\. Modularity (Plugins)

**Concept:** We don't want a "Spaghetti Code" situation where the Police System is tangled up with the Fishing System. If one breaks, everything breaks.

**The Solution:** We build **Plugins** (Modules).

- The "Core" mod handles saving, loading, and connecting.
- "Jobs," "Housing," and "Shops" are separate modules that plug into the Core.

**Why?** If a server owner wants a "No Laws" server, they can simply disable the `Life_LawModule` in the config, and the rest of the gamemode works perfectly without it.

* * *

## 6\. Network Optimization (The "Phonebook" System)

**Concept:** Sending text across the internet is heavy. Sending numbers is light.

- Text: `"{B45F...}Prefabs/Items/Food/Apple.et"` (Very long!)
- ID: `50` (Very short!)

**The Rule:** We use a **Registry**. When the server starts, it creates a list of all items and numbers them. It gives this list to the client.

1. **Server:** "Item #50 is an Apple."
2. **Client:** "Okay, I have written that down."
3. **Action:** When buying, the client just says "I want #50." The server knows that means Apple.

> **Important:** We only use these IDs while the game is running. When saving to the database, we switch back to the long text string. This ensures that if we add "Bananas" later and the IDs change, the save file doesn't get confused.

* * *

## 7\. Localization (Language)

**Concept:** Not everyone speaks English. We want our mod to be playable globally.

**The Rule:** Never write display text in your code.

- ❌ `Print("You bought an apple");`
- ✅ `Print("#Life_Msg_BoughtApple");`

**How it works:** We create `.st` (String Table) files. This is a spreadsheet where:

- `#Life_Msg_BoughtApple` = "You bought an apple" (English column)
- `#Life_Msg_BoughtApple` = "Du hast einen Apfel gekauft" (German column)

The game automatically picks the right column based on the player's settings.
