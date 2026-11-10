//! Harvest job crops. Sent with a Fruit Catcher reward claim
//! (LF_PlayerJobComponent.AskClaimFruitCatcherReward). Moved out of the unused
//! falling-fruit minigame widget, which loaded a layout that never existed.
//! Values are stable on the wire; 1 was TOMATOES (removed).
enum EHarvestJobType
{
	APPLES = 0,
	PLUMS = 2,
	BERRIES = 3
}
