// Ported from LNGCLifeRP_Public (MIT), author LNGCGenetic.
// Copyright (c) 2026 LNGCGenetic
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------------------------------
//! Placed jail marker. The cell origin is this entity, not a hard-coded world
//! position. Registers the zone, the sentence config, and the breach door, then
//! ticks confinement and the release clock on the server.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Placed jail cell. Origin is the sentence teleport and the confinement center.")]
class LF_JailCellComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_JailCellComponent : ScriptComponent
{
	[Attribute("8", UIWidgets.EditBox, desc: "Radius in meters a sentenced player is confined to around this entity.")]
	protected float m_fZoneRadius;

	[Attribute("60", UIWidgets.EditBox, desc: "Sentence seconds added for each wanted level.")]
	protected int m_iSecondsPerWanted;

	[Attribute("300", UIWidgets.EditBox, desc: "Maximum sentence seconds.")]
	protected int m_iMaxSeconds;

	[Attribute("1", UIWidgets.EditBox, desc: "Wanted levels removed when the sentence ends.")]
	protected int m_iWantedReduction;

	[Attribute("Roadmap_Jail_Door", UIWidgets.EditBox, desc: "Breach target id of the jail door. Confinement pauses while that door is open.")]
	protected string m_sDoorTargetId;

	[Attribute("1000", UIWidgets.EditBox, desc: "Milliseconds between confine and release checks.")]
	protected int m_iTickIntervalMs;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		if (!owner)
		{
			EL_Debug.Error("Jail", "cell component has no owner");
			return;
		}

		LF_Jail.Configure(m_iSecondsPerWanted, m_iMaxSeconds, m_iWantedReduction, m_fZoneRadius);
		LF_Jail.SetDoorTarget(m_sDoorTargetId);
		LF_Jail.SetCellOrigin(owner.GetOrigin());

		if (m_iTickIntervalMs <= 0)
		{
			EL_Debug.Error("Jail", "cell tick skipped: interval is not positive");
			return;
		}

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			EL_Debug.Error("Jail", "cell tick skipped: no callqueue");
			return;
		}

		GetGame().GetCallqueue().CallLater(OnTick, m_iTickIntervalMs, true);
		EL_Debug.Log("Jail", string.Format("cell tick scheduled ms=%1", m_iTickIntervalMs));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTick()
	{
		if (!Replication.IsServer())
			return;

		float elapsed = m_iTickIntervalMs * 0.001;
		LF_Jail.Tick(elapsed);
	}
}
