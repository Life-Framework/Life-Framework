// Ported from EveronLife (https://github.com/thebonbon/EveronLife), branch enhancement/Main_World.
// Original: src/Scripts/Game/Components/Vehicle/LF_SpikeTriggerComponent.c (the idea of a strip
// that zeroes the health of each wheel inside its box). Rewritten for #646: a deployed police strip,
// swept wheel paths instead of a trigger, the wheels' own hit zones on the wheel slot entities.
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
//! Police spike strip (#646). An on-duty officer deploys the folded strip (LF_SpikeStripDeployAction
//! on Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et): the placement service (#1026,
//! LF_PlacementService.Confirm, from the item's LF_PlaceableComponent) lays SpikeStrip.et across the
//! road in front of them, where their ghost showed it, on the ground. Each wheel
//! of a vehicle whose path crosses the strip is punctured through the vanilla wheel damage: its
//! SCR_WheelHitZone (on the wheel slot entity) takes damage until DESTROYED, and SCR_WheelHitZone
//! .UpdateWheelState shrinks the radius, cuts the grip and adds drag on every machine, so the car
//! slows and pulls to that side. The strip goes away after LF_PoliceConfig.SpikeStripLifetimeSeconds
//! or when an officer picks it up again (the folded item goes back into their inventory).
//! Server only: the check, the damage, the deploy and the pick-up. Clients see the replicated
//! entity and the replicated wheel damage.
//------------------------------------------------------------------------------------------------
//! The pure part: the swept wheel paths over the strip. LOGIC-tested by LF_Test_SpikeStripGeometry.
class LF_SpikeStripGeometry
{
	//------------------------------------------------------------------------------------------------
	//! True when the horizontal path from `from` to `to` touches the strip's footprint: the rectangle
	//! centred on `centre`, `halfLength` along the horizontal unit vector `axis` and `halfWidth`
	//! across it. A wheel moves several strip widths between two server ticks at speed, so the path
	//! is tested, not the point (Liang-Barsky clipping in the strip's own 2D frame).
	static bool SweepCrosses(vector centre, vector axis, float halfLength, float halfWidth, vector from, vector to)
	{
		vector side = Vector(-axis[2], 0, axis[0]);
		vector a = from - centre;
		vector b = to - centre;
		float u0 = a[0] * axis[0] + a[2] * axis[2];
		float v0 = a[0] * side[0] + a[2] * side[2];
		float u1 = b[0] * axis[0] + b[2] * axis[2];
		float v1 = b[0] * side[0] + b[2] * side[2];
		float du = u1 - u0;
		float dv = v1 - v0;
		float t0 = 0;
		float t1 = 1;
		if (!Clip(-du, u0 + halfLength, t0, t1))
			return false;
		if (!Clip(du, halfLength - u0, t0, t1))
			return false;
		if (!Clip(-dv, v0 + halfWidth, t0, t1))
			return false;

		return Clip(dv, halfWidth - v0, t0, t1);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Clip(float p, float q, inout float t0, inout float t1)
	{
		if (p == 0)
			return q >= 0;

		float r = q / p;
		if (p < 0)
		{
			if (r > t1)
				return false;
			if (r > t0)
				t0 = r;
			return true;
		}

		if (r < t0)
			return false;
		if (r < t1)
			t1 = r;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ground under a strip is usable when it rises or falls by at most maxStep along it: the
	//! placement tool's ground rule (docs/placement-tool.md "ground"). A kerb or a car is not.
	static bool GroundAcceptable(float lowest, float highest, float maxStep)
	{
		return highest - lowest <= maxStep;
	}
}

//------------------------------------------------------------------------------------------------
//! Server rules and actions: deploy (through LF_PlacementService), pick-up, the puncture.
class LF_SpikeStrips
{
	static const string FEATURE = "SpikeStrip";

	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	//! Deployed strips on this machine (the harness and the tests read them).
	static int Count()
	{
		return LF_SpikeStripComponent.CountAll();
	}

	//------------------------------------------------------------------------------------------------
	//! The deployed strip nearest to pos within radius, or null.
	static LF_SpikeStripComponent Nearest(vector pos, float radius)
	{
		return LF_SpikeStripComponent.NearestTo(pos, radius);
	}

	//------------------------------------------------------------------------------------------------
	static string LastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes every deployed strip (the world reset).
	static int DeleteAll()
	{
		return LF_SpikeStripComponent.DeleteAll();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_SpikeStripComponent.ResetAll();
		LF_SpikeStripPreview.ResetStatics();
		s_sLastRefusal = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer lays the strip `item` carries where the server plans it (no proposal: a
	//! server-side perform, the tests).
	//! \return the deployed strip, or null with the refusal logged and toasted.
	static IEntity Deploy(IEntity officer, IEntity item)
	{
		return Placed(officer, LF_PlacementService.ConfirmOwnSpot(officer, item));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer lays the strip `item` carries at `proposal`, the spot their ghost showed
	//! (LF_SpikeStripDeployAction's action data). LF_PlacementService.Confirm checks it again.
	static IEntity DeployAt(IEntity officer, IEntity item, vector proposal[4])
	{
		return Placed(officer, LF_PlacementService.Confirm(officer, item, proposal));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer folds the strip up; the item goes into their inventory, or onto the
	//! ground at their feet when it is full.
	static bool PickUp(IEntity officer, IEntity strip)
	{
		if (!Replication.IsServer() || !officer || !strip)
		{
			Refuse(officer, "pick-up refused: no officer or strip, or not the server", "");
			return false;
		}

		if (!LF_PoliceUtils.IsOnDutyCharacter(officer))
		{
			Refuse(officer, string.Format("pick-up refused: %1 is not on-duty police", LF_PoliceRanks.ResolveUid(officer)), "#LF-SpikeStrip_PoliceOnly");
			return false;
		}

		LF_SpikeStripComponent component = LF_Component<LF_SpikeStripComponent>.Find(strip);
		if (!component || component.GetItemPrefab().IsEmpty())
		{
			Refuse(officer, "pick-up refused: the strip names no item prefab", "");
			return false;
		}

		if (!LF_RateLimit.AllowCharacter(officer, LF_ERequestKind.SPAWN, "spike strip pick-up"))
		{
			s_sLastRefusal = "pick-up refused: rate limit";
			return false;
		}

		int added = LF_InventoryUtils.AddAmount(officer, component.GetItemPrefab(), 1, true);
		LF_Debug.Info(FEATURE, string.Format("picked up by %1 (into inventory %2)", LF_PoliceRanks.ResolveUid(officer), added));
		SCR_EntityHelper.DeleteEntityAndChildren(strip);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The strip the service placed, with its deployer set; or null with the service's refusal kept.
	protected static IEntity Placed(IEntity officer, IEntity strip)
	{
		if (!strip)
		{
			s_sLastRefusal = "deploy " + LF_PlacementService.LastRefusal();
			return null;
		}

		LF_SpikeStripComponent component = LF_Component<LF_SpikeStripComponent>.Find(strip);
		if (component)
			component.SetDeployer(LF_PoliceRanks.ResolveUid(officer));

		vector transform[4];
		strip.GetWorldTransform(transform);
		LF_Debug.Info(FEATURE, string.Format("deployed by %1 at %2 heading %3, lasts %4 s", LF_PoliceRanks.ResolveUid(officer), transform[3], transform[2], LF_PoliceConfig.SpikeStripLifetimeSeconds()));
		return strip;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the vanilla wheel damage, until the wheel is DESTROYED (a flat tyre on the rim).
	//! Falls back to SetHealth(0) when the damage did not take (a damage-disabled vehicle), and says so.
	static bool Puncture(SCR_WheelHitZone wheel, IEntity strip, string vehicleName)
	{
		if (!wheel || !Replication.IsServer())
			return false;

		if (wheel.GetDamageState() == EDamageState.DESTROYED)
			return false;

		IEntity wheelEntity = wheel.GetOwner();
		SCR_DamageManagerComponent damage;
		if (wheelEntity)
			damage = SCR_DamageManagerComponent.Cast(wheelEntity.FindComponent(SCR_DamageManagerComponent));

		if (damage && wheelEntity)
		{
			vector hitPosDirNorm[3];
			hitPosDirNorm[0] = wheelEntity.GetOrigin();
			hitPosDirNorm[1] = vector.Up;
			hitPosDirNorm[2] = vector.Up;
			// COLLISION: a tyre run over spikes, not a shot. Enough to cross any wheel's reduction.
			float amount = wheel.GetMaxHealth() * 10;
			SCR_DamageContext context = new SCR_DamageContext(EDamageType.COLLISION, amount, hitPosDirNorm, wheelEntity, wheel, Instigator.CreateInstigator(null), null, -1, -1);
			context.damageSource = strip;
			damage.HandleDamage(context);
		}

		if (wheel.GetDamageState() != EDamageState.DESTROYED)
		{
			LF_Debug.Warn(FEATURE, string.Format("wheel %1 of %2 took no damage (health %3); setting it to zero", wheel.GetWheelIndex(), vehicleName, wheel.GetHealth()));
			wheel.SetHealth(0);
		}

		LF_Debug.Info(FEATURE, string.Format("punctured wheel %1 of %2 (state %3)", wheel.GetWheelIndex(), vehicleName, wheel.GetDamageState()));
		return wheel.GetDamageState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! The wheel hit zones of a vehicle (on its wheel slot entities), with a wheel index set.
	static void Wheels(IEntity vehicle, notnull array<SCR_WheelHitZone> wheels)
	{
		wheels.Clear();
		if (!vehicle)
			return;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (!damage)
			return;

		array<HitZone> zones = {};
		damage.GetAllHitZonesInHierarchy(zones);
		AddWheels(zones, wheels);
		if (!wheels.IsEmpty())
			return;

		// The wheels are slot entities with their own damage manager (S105_wheel_01.et): walk them.
		IEntity child = vehicle.GetChildren();
		while (child)
		{
			DamageManagerComponent childDamage = DamageManagerComponent.Cast(child.FindComponent(DamageManagerComponent));
			if (childDamage)
			{
				zones.Clear();
				childDamage.GetAllHitZones(zones);
				AddWheels(zones, wheels);
			}

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddWheels(notnull array<HitZone> zones, notnull array<SCR_WheelHitZone> wheels)
	{
		foreach (HitZone zone : zones)
		{
			SCR_WheelHitZone wheel = SCR_WheelHitZone.Cast(zone);
			if (wheel && wheel.GetWheelIndex() >= 0 && wheel.GetOwner() && wheels.Find(wheel) < 0)
				wheels.Insert(wheel);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle for a log line: its name, else its prefab.
	static string Describe(IEntity vehicle)
	{
		if (!vehicle)
			return "nothing";

		if (!vehicle.GetName().IsEmpty())
			return vehicle.GetName();

		EntityPrefabData data = vehicle.GetPrefabData();
		if (data)
			return data.GetPrefabName();

		return "a vehicle";
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Refuse(IEntity officer, string message, string toastKey)
	{
		s_sLastRefusal = message;
		LF_Debug.Warn(FEATURE, message);
		if (toastKey.IsEmpty() || !officer || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", toastKey, 3.0, LF_ENotificationType.WARNING);

		return null;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Police", description: "Deployed spike strip: punctures the wheels that cross it, then despawns.")]
class LF_SpikeStripComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One vehicle near a strip: where each of its wheels was at the last tick.
class LF_SpikeStripTrack
{
	EntityID m_Vehicle;
	ref array<vector> m_aLast = {};
	ref array<int> m_aIndex = {};
	bool m_bSeen;
}

//------------------------------------------------------------------------------------------------
class LF_SpikeStripComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Carried item an officer gets back when picking the strip up", "et")]
	protected ResourceName m_sItemPrefab;

	[Attribute("1.75", UIWidgets.Auto, "Half the strip's length (metres, along the entity's forward axis)")]
	protected float m_fHalfLength;

	[Attribute("0.15", UIWidgets.Auto, "Half the strip's width (metres, across it)")]
	protected float m_fHalfWidth;

	[Attribute("1", UIWidgets.CheckBox, "Despawn after LF_PoliceConfig.SpikeStripLifetimeSeconds. Off for a strip placed in a world layer")]
	protected bool m_bExpires;

	[Attribute("1", UIWidgets.CheckBox, "Puncture the wheels that cross it. Off for a display strip in a world layer")]
	protected bool m_bArmed;

	//! Server tick of the wheel check.
	static const int TICK_MS = 50; // lf-fixed: sampling period of a physical check, not a gameplay knob
	//! A wheel centre this far above or below the strip's middle does not touch it.
	static const float REACH_UP = 1.5; // lf-fixed: geometry of a wheel on the strip
	//! Vehicles are looked for inside this distance of the strip's middle, beyond its half length.
	static const float QUERY_MARGIN = 8; // lf-fixed: geometry, larger than any car

	//! Every strip on this machine; OnPostInit adds, OnDelete removes.
	protected static ref array<LF_SpikeStripComponent> s_aAll;

	protected string m_sDeployer;
	protected ref array<ref LF_SpikeStripTrack> m_aTracks = {};
	protected ref array<IEntity> m_aFound = {};
	protected int m_iPunctures;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aAll)
			s_aAll = new array<LF_SpikeStripComponent>();
		if (s_aAll.Find(this) < 0)
			s_aAll.Insert(this);

		if (!Replication.IsServer() || !GetGame() || !GetGame().InPlayMode())
			return;

		if (m_bArmed)
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		if (m_bExpires)
			GetGame().GetCallqueue().CallLater(Expire, LF_PoliceConfig.SpikeStripLifetimeSeconds() * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aAll)
			s_aAll.RemoveItem(this);

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(Expire);
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static int CountAll()
	{
		if (!s_aAll)
			return 0;

		return s_aAll.Count();
	}

	//------------------------------------------------------------------------------------------------
	static LF_SpikeStripComponent NearestTo(vector pos, float radius)
	{
		LF_SpikeStripComponent best;
		float bestDistance = radius;
		if (!s_aAll)
			return null;

		foreach (LF_SpikeStripComponent strip : s_aAll)
		{
			if (!strip || !strip.GetOwner())
				continue;

			float d = vector.Distance(strip.GetOwner().GetOrigin(), pos);
			if (d <= bestDistance)
			{
				bestDistance = d;
				best = strip;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		s_aAll = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: picks every strip an officer deployed off the road (the world reset); a placed one stays.
	//! \return How many strips were removed.
	static int DeleteAll()
	{
		if (!s_aAll)
			return 0;

		array<IEntity> owners = {};
		foreach (LF_SpikeStripComponent strip : s_aAll)
		{
			// Only strips an officer laid (they expire); the one placed in the town stays.
			if (strip && strip.m_bExpires && strip.GetOwner())
				owners.Insert(strip.GetOwner());
		}

		foreach (IEntity owner : owners)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(owner);
		}

		return owners.Count();
	}

	//------------------------------------------------------------------------------------------------
	void SetDeployer(string uid)
	{
		m_sDeployer = uid;
	}

	//------------------------------------------------------------------------------------------------
	string GetDeployer()
	{
		return m_sDeployer;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetItemPrefab()
	{
		return m_sItemPrefab;
	}

	//------------------------------------------------------------------------------------------------
	int GetPunctures()
	{
		return m_iPunctures;
	}

	//------------------------------------------------------------------------------------------------
	protected void Expire()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		LF_Debug.Info(LF_SpikeStrips.FEATURE, string.Format("expired after %1 s (deployed by %2, %3 punctures)", LF_PoliceConfig.SpikeStripLifetimeSeconds(), m_sDeployer, m_iPunctures));
		SCR_EntityHelper.DeleteEntityAndChildren(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one sample of every vehicle near the strip. Public for the tests, which move a car
	//! between two calls.
	void Tick()
	{
		IEntity owner = GetOwner();
		if (!owner || !GetGame() || !GetGame().GetWorld())
			return;

		foreach (LF_SpikeStripTrack track : m_aTracks)
		{
			track.m_bSeen = false;
		}

		m_aFound.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(owner.GetOrigin(), m_fHalfLength + QUERY_MARGIN, CollectVehicle, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity vehicle : m_aFound)
		{
			CheckVehicle(vehicle);
		}

		// A vehicle that drove off is forgotten; it starts a new path if it comes back.
		for (int i = m_aTracks.Count() - 1; i >= 0; i--)
		{
			if (!m_aTracks[i].m_bSeen)
				m_aTracks.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectVehicle(IEntity entity)
	{
		if (Vehicle.Cast(entity))
			m_aFound.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckVehicle(IEntity vehicle)
	{
		IEntity owner = GetOwner();
		LF_SpikeStripTrack track = TrackFor(vehicle.GetID());
		track.m_bSeen = true;

		array<SCR_WheelHitZone> wheels = {};
		LF_SpikeStrips.Wheels(vehicle, wheels);
		vector transform[4];
		owner.GetWorldTransform(transform);
		vector axis = LF_PlacementGeometry.Flat(transform[2]);
		vector centre = transform[3];
		foreach (SCR_WheelHitZone wheel : wheels)
		{
			vector now = wheel.GetOwner().GetOrigin();
			int at = track.m_aIndex.Find(wheel.GetWheelIndex());
			if (at < 0)
			{
				track.m_aIndex.Insert(wheel.GetWheelIndex());
				track.m_aLast.Insert(now);
				continue;
			}

			vector before = track.m_aLast[at];
			track.m_aLast[at] = now;
			if (Math.AbsFloat(now[1] - centre[1]) > REACH_UP)
				continue;

			if (!LF_SpikeStripGeometry.SweepCrosses(centre, axis, m_fHalfLength, m_fHalfWidth, before, now))
				continue;

			if (LF_SpikeStrips.Puncture(wheel, owner, LF_SpikeStrips.Describe(vehicle)))
				m_iPunctures++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected LF_SpikeStripTrack TrackFor(EntityID vehicle)
	{
		foreach (LF_SpikeStripTrack track : m_aTracks)
		{
			if (track.m_Vehicle == vehicle)
				return track;
		}

		LF_SpikeStripTrack added = new LF_SpikeStripTrack();
		added.m_Vehicle = vehicle;
		m_aTracks.Insert(added);
		return added;
	}
}
