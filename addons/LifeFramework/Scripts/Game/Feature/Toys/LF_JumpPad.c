// Ported from EveronLife (https://github.com/thebonbon/EveronLife), branch enhancement/Main_World.
// Original: src/Scripts/Game/Components/Vehicle/EL_JumpTrigger.c
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
//! Jump pad. Entering applies an upward velocity once. Leaving clears that enter
//! so the next entry can launch again. A disabled pad applies the zero vector.
//------------------------------------------------------------------------------------------------
class LF_JumpPad
{
	static const float DEFAULT_UPWARD_SPEED = 12;

	protected bool m_bEnabled = true;
	protected float m_fUpwardSpeed = DEFAULT_UPWARD_SPEED;
	protected ref array<int> m_aAppliedVisitors = {};

	//------------------------------------------------------------------------------------------------
	void SetEnabled(bool enabled)
	{
		m_bEnabled = enabled;
		if (enabled)
			EL_Debug.Log("JumpPad", "enabled");
		else
			EL_Debug.Log("JumpPad", "disabled");
	}

	//------------------------------------------------------------------------------------------------
	void SetUpwardSpeed(float speed)
	{
		m_fUpwardSpeed = speed;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEnabled()
	{
		return m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Velocity written for this enter. Zero when the pad is disabled, the speed
	//! is not upward, or this visitor was already launched and has not left.
	vector AcceptEnter(int visitorId)
	{
		if (visitorId <= 0)
		{
			EL_Debug.Error("JumpPad", "enter rejected: visitor is missing");
			return Vector(0, 0, 0);
		}

		if (!m_bEnabled)
		{
			EL_Debug.Log("JumpPad", "rejected: pad disabled");
			return Vector(0, 0, 0);
		}

		if (m_fUpwardSpeed <= 0)
		{
			EL_Debug.Error("JumpPad", "enter skipped: upward speed is not positive");
			return Vector(0, 0, 0);
		}

		if (!m_aAppliedVisitors)
			m_aAppliedVisitors = new array<int>();

		int index = m_aAppliedVisitors.Find(visitorId);
		if (index != -1)
		{
			EL_Debug.Log("JumpPad", "rejected: already launched this enter");
			return Vector(0, 0, 0);
		}

		m_aAppliedVisitors.Insert(visitorId);
		EL_Debug.Log("JumpPad", string.Format("launched visitor %1 at %2", visitorId, m_fUpwardSpeed));
		return Vector(0, m_fUpwardSpeed, 0);
	}

	//------------------------------------------------------------------------------------------------
	void AcceptExit(int visitorId)
	{
		if (!m_aAppliedVisitors)
			return;

		int index = m_aAppliedVisitors.Find(visitorId);
		if (index == -1)
			return;

		m_aAppliedVisitors.Remove(index);
		EL_Debug.Log("JumpPad", string.Format("visitor %1 left", visitorId));
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Toys", description: "Trigger that sets a player's velocity upward once when they enter.")]
class LF_JumpPadComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_JumpPadComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, desc: "When disabled, entering applies no velocity.")]
	protected bool m_bEnabled = true;

	[Attribute("12", UIWidgets.EditBox, desc: "Upward speed in meters per second, applied once per enter.")]
	protected float m_fUpwardSpeed = 12;

	protected ref LF_JumpPad m_Pad;
	protected ref array<EntityID> m_aSeenIds;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!m_Pad)
			m_Pad = new LF_JumpPad();

		m_Pad.SetEnabled(m_bEnabled);
		m_Pad.SetUpwardSpeed(m_fUpwardSpeed);

		if (!owner)
		{
			EL_Debug.Error("JumpPad", "init skipped: no owner");
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
			EL_Debug.Error("JumpPad", "init skipped: no owner");
			return;
		}

		SCR_BaseTriggerEntity trigger = SCR_BaseTriggerEntity.Cast(owner);
		if (!trigger)
		{
			EL_Debug.Error("JumpPad", "init skipped: owner is not a trigger");
			return;
		}

		ScriptInvoker activate = trigger.GetOnActivate();
		if (!activate)
		{
			EL_Debug.Error("JumpPad", "init skipped: no activate invoker");
			return;
		}

		activate.Insert(OnPlayerEnter);

		ScriptInvoker deactivate = trigger.GetOnDeactivate();
		if (!deactivate)
		{
			EL_Debug.Error("JumpPad", "exit will not clear: no deactivate invoker");
			return;
		}

		deactivate.Insert(OnPlayerExit);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_JumpPad Pad()
	{
		if (!m_Pad)
			m_Pad = new LF_JumpPad();

		return m_Pad;
	}

	//------------------------------------------------------------------------------------------------
	protected int TokenFor(IEntity ent, bool create)
	{
		if (!ent)
			return 0;

		EntityID id = ent.GetID();
		if (id == EntityID.INVALID)
		{
			EL_Debug.Error("JumpPad", "enter rejected: invalid entity id");
			return 0;
		}

		if (!m_aSeenIds)
			m_aSeenIds = new array<EntityID>();

		int index = m_aSeenIds.Find(id);
		if (index >= 0)
			return index + 1;

		if (!create)
			return 0;

		m_aSeenIds.Insert(id);
		return m_aSeenIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	void OnPlayerEnter(IEntity ent)
	{
		if (!ent)
		{
			EL_Debug.Error("JumpPad", "enter rejected: no entity");
			return;
		}

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
		if (!character)
			return;

		int token = TokenFor(ent, true);
		if (token <= 0)
			return;

		vector velocity = Pad().AcceptEnter(token);
		if (velocity == Vector(0, 0, 0))
			return;

		Physics phys = ent.GetPhysics();
		if (!phys)
		{
			EL_Debug.Error("JumpPad", "enter skipped: player has no physics");
			Pad().AcceptExit(token);
			return;
		}

		phys.SetActive(ActiveState.ACTIVE);
		phys.SetVelocity(velocity);
	}

	//------------------------------------------------------------------------------------------------
	void OnPlayerExit(IEntity ent)
	{
		if (!ent)
			return;

		int token = TokenFor(ent, false);
		if (token <= 0)
			return;

		Pad().AcceptExit(token);
	}
}
