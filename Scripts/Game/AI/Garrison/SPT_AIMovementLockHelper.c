// version date 06-13-26
// SPT AI Garrison






// pins garrisoned soldiers in place, fire and perception stay live, they just wont move
class SPT_AIMovementLockHelper
{
	// every1 that is static so they can be ungarrison'd
	protected static ref set<IEntity> s_LockedCharacters;

	// freeze or release singles
	static void ApplyLockState(IEntity character, bool locked)
	{
		if (!character)
			return;

		CharacterControllerComponent cc = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (!cc)
			return;

		cc.SetDisableMovementControls(locked);

		if (!s_LockedCharacters)
			s_LockedCharacters = new set<IEntity>();

		if (locked)
			s_LockedCharacters.Insert(character);
		else
			s_LockedCharacters.RemoveItem(character);
	}

	static bool IsLocked(IEntity character)
	{
		if (!character || !s_LockedCharacters)
			return false;
		return s_LockedCharacters.Contains(character);
	}

	// lift everyone we're holding
	static void UnlockAll()
	{
		if (!s_LockedCharacters)
			return;

		foreach (IEntity character : s_LockedCharacters)
		{
			if (!character)
				continue;
			CharacterControllerComponent cc = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
			if (cc)
				cc.SetDisableMovementControls(false);
		}
		s_LockedCharacters.Clear();
	}
}
