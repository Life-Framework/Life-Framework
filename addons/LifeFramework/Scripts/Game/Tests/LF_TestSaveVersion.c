//------------------------------------------------------------------------------------------------
//! The save version the serializer writes today (LF_PersistenceComponentSerializer.SerializeStore),
//! as the persistence tests expect it. A payload appended to RegisterPersistencePayloads raises it
//! by one here and nowhere else in the tests. Kept apart from the serializer on purpose: a test that
//! read the serializer's own number could not catch a wrong one.
//------------------------------------------------------------------------------------------------
class LF_TestSaveVersion
{
	static const int CURRENT = 41;
}
