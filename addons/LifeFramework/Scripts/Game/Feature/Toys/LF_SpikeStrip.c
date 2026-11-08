// Ported from EveronLife (https://github.com/thebonbon/EveronLife), branch enhancement/Main_World.
// Original: src/Scripts/Game/Components/Vehicle/EL_SpikeTriggerComponent.c
// Copyright (c) 2022 EveronLife
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
//! Spike strip. A wheeled vehicle that enters loses each wheel whose position
//! sits inside the strip box: that wheel's hit zone health is set to zero.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Toys", description: "Trigger that destroys vehicle wheels inside the strip box.")]
class LF_SpikeStripClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_SpikeStrip : ScriptComponent
{
	[Attribute("0 0 0", UIWidgets.EditBox, desc: "Local-space minimum of the strip box.")]
	protected vector m_vMins;

	[Attribute("0 0 0", UIWidgets.EditBox, desc: "Local-space maximum of the strip box.")]
	protected vector m_vMaxs;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!owner)
		{
			EL_Debug.Error("SpikeStrip", "init skipped: no owner");
			return;
		}

		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, false);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!owner)
		{
			EL_Debug.Error("SpikeStrip", "init skipped: no owner");
			return;
		}

		SCR_BaseTriggerEntity trigger = SCR_BaseTriggerEntity.Cast(owner);
		if (!trigger)
		{
			EL_Debug.Error("SpikeStrip", "init skipped: owner is not a trigger");
			return;
		}

		ScriptInvoker activate = trigger.GetOnActivate();
		if (!activate)
		{
			EL_Debug.Error("SpikeStrip", "init skipped: no activate invoker");
			return;
		}

		activate.Insert(OnVehicleEnter);
	}

	//------------------------------------------------------------------------------------------------
	void OnVehicleEnter(IEntity ent)
	{
		if (!ent)
		{
			EL_Debug.Error("SpikeStrip", "enter rejected: no entity");
			return;
		}

		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(ent.FindComponent(VehicleWheeledSimulation));
		if (!simulation)
			return;

		SCR_VehicleDamageManagerComponent damageManager = SCR_VehicleDamageManagerComponent.Cast(ent.FindComponent(SCR_VehicleDamageManagerComponent));
		if (!damageManager)
		{
			EL_Debug.Error("SpikeStrip", "vehicle has no damage manager");
			return;
		}

		IEntity owner = GetOwner();
		if (!owner)
		{
			EL_Debug.Error("SpikeStrip", "enter skipped: no owner");
			return;
		}

		vector mins = owner.CoordToParent(m_vMins);
		vector maxs = owner.CoordToParent(m_vMaxs);
		array<HitZone> hitZones = {};
		damageManager.GetAllHitZones(hitZones);

		int wheelCount = simulation.WheelCount();
		if (wheelCount <= 0)
		{
			EL_Debug.Error("SpikeStrip", "vehicle has no wheels");
			return;
		}

		for (int i = 0; i < wheelCount; i++)
		{
			vector wheelPos = simulation.WheelGetPosition(i);
			wheelPos = ent.CoordToParent(wheelPos);

			bool inStrip = Math3D.IntersectionBoxBox(mins, maxs, wheelPos, wheelPos);
			if (!inStrip)
				continue;

			SCR_WheelHitZone wheel = FindWheel(hitZones, simulation, i);
			if (!wheel)
			{
				EL_Debug.Error("SpikeStrip", string.Format("wheel hit zone missing for index %1", i));
				continue;
			}

			wheel.SetHealth(0);
			EL_Debug.Log("SpikeStrip", string.Format("wheel spiked %1", i));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_WheelHitZone FindWheel(array<HitZone> hitZones, VehicleWheeledSimulation simulation, int wheelIndex)
	{
		if (!hitZones || !simulation)
			return null;

		foreach (HitZone hitZone : hitZones)
		{
			if (!hitZone)
				continue;

			if (hitZone.Type() != SCR_WheelHitZone)
				continue;

			int index = simulation.WheelGetIndex(hitZone.GetName());
			if (index != wheelIndex)
				continue;

			return SCR_WheelHitZone.Cast(hitZone);
		}

		return null;
	}
}
