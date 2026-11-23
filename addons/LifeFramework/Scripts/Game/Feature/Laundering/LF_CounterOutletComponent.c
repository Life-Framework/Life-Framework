[ComponentEditorProps(category: "LifeFramework/Laundering", description: "Under-the-counter outlet: the barkeeper buys the goods its demand rows name (#4200)")]
class LF_CounterOutletComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marks a barkeeper (or a bar counter) as an under-the-counter outlet (#4200). The buyer id is the half
//! of the demand-row key (LF_BuyerDemandConfig) the pack writes for this bar, so which goods, the weekly
//! cap and the price live in the pack's rows, not here. Pair it with LF_CounterOutletAction.
class LF_CounterOutletComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Demand-row buyer id of this outlet (a building id the pack picks)")]
	string m_sBuyerId;

	[Attribute("3", UIWidgets.Auto, "Meters the seller may stand from the barkeeper", params: "0.5 10")]
	float m_fReach;

	[Attribute("1500", UIWidgets.Auto, "Shortest gap between two sale attempts by one seller, in ms", params: "0 60000")]
	int m_iMinActionMs;
}
