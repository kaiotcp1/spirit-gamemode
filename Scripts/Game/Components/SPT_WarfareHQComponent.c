//! Componente exclusivo do HQ aliado. Nao possui configuracao hostil.
[ComponentEditorProps(category: "SPT/Warfare", description: "Define um HQ aliado do modo SPT Warfare.")]
class SPT_WarfareHQComponentClass : SPT_WarfareNodeComponentClass
{
}

class SPT_WarfareHQComponent : SPT_WarfareNodeComponent
{
	override int GetCaptureOrder() { return 0; }
	override bool IsHQ() { return true; }
	override bool CountsForVictory() { return false; }
}
