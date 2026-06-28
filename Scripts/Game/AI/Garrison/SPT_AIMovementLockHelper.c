// data da versao 06-13-26
// SPT AI Garrison – auxiliar de trava de movimento para soldados guarnecidos

// Mantem soldados guarnecidos fixos no posto designado. O disparo e a percepcao
// permanecem ativos; apenas a locomocao fica desabilitada enquanto estiverem guarnecidos.
class SPT_AIMovementLockHelper
{
	// Conjunto estatico com todos os personagens atualmente travados, usado para liberacao em lote
	protected static ref set<IEntity> s_LockedCharacters;

	// Aplica ou remove a trava de movimento de um unico personagem
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

	// Verifica se um personagem esta atualmente travado
	static bool IsLocked(IEntity character)
	{
		if (!character || !s_LockedCharacters)
			return false;
		return s_LockedCharacters.Contains(character);
	}

	// Libera todos os personagens travados de uma vez (limpeza geral)
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
