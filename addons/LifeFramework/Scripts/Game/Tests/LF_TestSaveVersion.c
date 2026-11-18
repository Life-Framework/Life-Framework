//------------------------------------------------------------------------------------------------
//! The save version the serializer writes today (LF_PersistenceComponentSerializer.SerializeStore),
//! as the persistence tests expect it. Kept apart from the serializer on purpose: a test that read
//! the serializer's own number could not catch a wrong one.
//! Since #2841 the store writes the sectioned format (CURRENT) after the frozen legacy payloads
//! (LEGACY of them). A new payload is a section with its own version: neither number moves.
//------------------------------------------------------------------------------------------------
class LF_TestSaveVersion
{
	static const int CURRENT = 1000;
	static const int LEGACY = 57;
}
