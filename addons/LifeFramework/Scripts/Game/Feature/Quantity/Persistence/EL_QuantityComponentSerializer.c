//------------------------------------------------------------------------------------------------
//! Persists the per-item stack quantity (EL_QuantityComponent), replacing the old EPF component
//! save data. Mirrors the previous behaviour exactly: a stack of 1 is the default and is NOT
//! written; on load, a restored quantity keeps the one-frame transfer intent so stacks restore
//! separately during the load-into-storages situation.
//!
//! BINDING. Listed in the ComponentSerializers block of Configs/Systems/Persistence/LifeFramework.conf
//! under the item entity configs that carry EL_QuantityComponent.
//!
//! FORMAT. Version first, then the quantity only when it differs from the default of 1. Skipping
//! the write for a default stack means the load side must treat "no quantity present" and
//! "quantity present but <= 1" identically: leave the component alone.
//------------------------------------------------------------------------------------------------
class EL_QuantityComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return EL_QuantityComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the stack quantity when it differs from the default of 1.
	//! \param[in] owner The item entity owning the quantity component.
	//! \param[in] component The quantity component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an EL_QuantityComponent.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		EL_QuantityComponent quantity = EL_QuantityComponent.Cast(component);
		if (!quantity)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// Quantity 1 is the default - persisting it would be dead weight.
		int amount = quantity.GetQuantity();
		if (amount == 1)
			return ESerializeResult.OK;

		context.Write(amount);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the stack quantity, keeping the one-frame transfer intent that keeps stacks
	//! separate during load.
	//! \param[in] owner The item entity owning the quantity component.
	//! \param[in] component The quantity component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		EL_QuantityComponent quantity = EL_QuantityComponent.Cast(component);
		if (!quantity)
			return false;

		// No version means this component has no payload in the stored record.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// A default stack (1) is never written, so nothing to read means "default". Reading past
		// the end yields a failed read and/or a zero value - both mean "leave the stack alone".
		int amount;
		if (!context.Read(amount) || amount <= 1)
			return true;

		quantity.SetQuantity(amount);

		// Keep separate for current frame to have (relevant during load into storages situation).
		// Remove intent on next frame
		EL_QuantityComponent.SetTransferIntent(quantity.GetOwner(), true);
		GetGame().GetCallqueue().Call(quantity.RemoveTransferIntent, quantity.GetOwner());

		return true;
	}
}