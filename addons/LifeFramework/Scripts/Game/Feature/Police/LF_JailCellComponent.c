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

	[Attribute("", UIWidgets.EditBox, desc: "Breach target id of the jail door (its LF_BreachTargetComponent id). Confinement pauses while that door is open. Empty: no door gates the sentence.")]
	protected string m_sDoorTargetId;

	[Attribute("0 0 -5", UIWidgets.EditBox, desc: "Where a released prisoner stands, in this entity's local frame (outside the cell door).")]
	protected vector m_vReleaseOffset;

	[Attribute("1000", UIWidgets.EditBox, desc: "Milliseconds between confine and release checks.")]
	protected int m_iTickIntervalMs;

	//! LF_Jail config and its tick are process-global, so exactly one cell owns them. A second
	//! placed cell would otherwise overwrite the config and add a second repeating Tick
	//! (two cells = sentences run at double speed).
	protected static LF_JailCellComponent s_TickOwner;
	protected bool m_bTicking;

	//! The first cell placed in the world. LF_Jail.SyncPlacedCell re-applies its door target and origin after a reset.
	protected static LF_JailCellComponent s_Placed;

	//------------------------------------------------------------------------------------------------
	//! The first placed cell, or null when the world has none. One per world.
	static LF_JailCellComponent GetPlaced()
	{
		return s_Placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applies the placed cell's door target and origin to LF_Jail.
	//! Returns false when no placed cell exists.
	static bool ReapplyPlaced()
	{
		if (!s_Placed || !s_Placed.GetOwner())
			return false;

		s_Placed.ApplyPlacement(s_Placed.GetOwner());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPlacement(IEntity owner)
	{
		if (!m_sDoorTargetId.IsEmpty())
			LF_Jail.SetDoorTarget(m_sDoorTargetId);

		LF_Jail.SetCellOrigin(owner.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this cell owns the jail config and the repeating LF_Jail.Tick.
	bool IsTickOwner()
	{
		return s_TickOwner == this && m_bTicking;
	}

	//------------------------------------------------------------------------------------------------
	//! Game-mode start/end reset (LF_StaticReset). Placed cells re-register on the next world load.
	static void ResetStatics()
	{
		s_TickOwner = null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		if (!owner)
		{
			LF_Debug.Error("Jail", "cell component has no owner");
			return;
		}

		if (s_TickOwner && s_TickOwner != this)
		{
			LF_Debug.Error("Jail", string.Format("second jail cell ignored at %1: the first placed cell owns the config and tick", owner.GetOrigin()));
			return;
		}

		s_TickOwner = this;
		s_Placed = this;
		// The jail's boot check finds the cell by component (#1047).
		LF_Places.Register(LF_Jail.PLACE_KIND, LF_Jail.PLACE_ID, owner);
		LF_Jail.Configure(m_iSecondsPerWanted, m_iMaxSeconds, m_iWantedReduction, m_fZoneRadius);
		ApplyPlacement(owner);
		LF_Jail.SetReleaseOffset(owner.CoordToParent(m_vReleaseOffset) - owner.GetOrigin());

		if (m_iTickIntervalMs <= 0)
		{
			LF_Debug.Error("Jail", "cell tick skipped: interval is not positive");
			return;
		}

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			LF_Debug.Error("Jail", "cell tick skipped: no callqueue");
			return;
		}

		GetGame().GetCallqueue().CallLater(OnTick, m_iTickIntervalMs, true);
		m_bTicking = true;
		LF_Debug.Log("Jail", string.Format("cell tick scheduled ms=%1", m_iTickIntervalMs));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bTicking)
		{
			m_bTicking = false;
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(OnTick);

			LF_Debug.Log("Jail", "cell tick removed");
		}

		if (s_TickOwner == this)
			s_TickOwner = null;

		if (s_Placed == this)
			s_Placed = null;

		super.OnDelete(owner);
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
