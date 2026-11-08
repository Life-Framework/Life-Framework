//! Harvest job crops. Sent with a Fruit Catcher reward claim
//! (EL_PlayerJobComponent.AskClaimFruitCatcherReward). Moved out of the unused
//! falling-fruit minigame widget, which loaded a layout that never existed.
enum EHarvestJobType
{
	APPLES,
	TOMATOES,
	PLUMS,
	BERRIES
}
