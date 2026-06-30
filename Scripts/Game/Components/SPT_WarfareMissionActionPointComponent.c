//! Desenha a posicao real de uma action de missao quando o debug da
//! guarnicao esta habilitado no GameMode.
[ComponentEditorProps(category: "SPT/Warfare/Mission", description: "Marca visualmente um ponto de action durante debug.")]
class SPT_WarfareMissionActionPointComponentClass : ScriptComponentClass
{
}

class SPT_WarfareMissionActionPointComponent : ScriptComponent
{
	protected const int DEBUG_DRAW_INTERVAL_MS = 250;
	protected bool m_bDebugMarkerLogged;
	protected ref Shape m_DebugSphere;
	protected ref Shape m_DebugVerticalLine;

	[Attribute("0 1 0", desc: "Offset visual do marcador em relacao ao proxy.")]
	protected vector m_vMarkerOffset;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		GetGame().GetCallqueue().CallLater(DrawDebugMarker, DEBUG_DRAW_INTERVAL_MS, true);
	}

	void ~SPT_WarfareMissionActionPointComponent()
	{
		GetGame().GetCallqueue().Remove(DrawDebugMarker);
		ClearDebugMarker();
	}

	protected void DrawDebugMarker()
	{
		SPT_WorldGarrisonManagerComponent garrison = SPT_WorldGarrisonManagerComponent.GetInstance();
		if (!garrison || !garrison.IsDebugEnabled())
		{
			ClearDebugMarker();
			return;
		}

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
		{
			ClearDebugMarker();
			return;
		}

		// As formas sem ShapeFlags.ONCE permanecem visiveis enquanto estas
		// referencias existirem. Recria-las periodicamente causava flicker.
		if (m_DebugSphere && m_DebugVerticalLine)
			return;

		int color = ARGB(255, 255, 0, 255);
		vector position = owner.GetOrigin() + m_vMarkerOffset;
		if (!m_bDebugMarkerLogged)
		{
			m_bDebugMarkerLogged = true;
			Print(string.Format("[SPT_WarfareMission] Marcador debug da action ativo | posicao=%1",
				position));
		}

		m_DebugSphere = Shape.CreateSphere(
			color,
			ShapeFlags.NOZBUFFER,
			position,
			0.75);

		vector verticalLine[2];
		verticalLine[0] = position;
		verticalLine[1] = position + Vector(0, 4.0, 0);
		m_DebugVerticalLine = Shape.CreateLines(
			color,
			ShapeFlags.NOZBUFFER,
			verticalLine,
			2);
	}

	protected void ClearDebugMarker()
	{
		m_DebugSphere = null;
		m_DebugVerticalLine = null;
		m_bDebugMarkerLogged = false;
	}
}
