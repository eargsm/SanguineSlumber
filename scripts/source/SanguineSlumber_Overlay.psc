Scriptname SanguineSlumber_Overlay Hidden

bool Function IsOwnTexture(string asTexture) global
	return StringUtil.Find(asTexture, "SanguineSlumber") >= 0
EndFunction

string Function DefaultTexture() global
	return "actors\\character\\overlays\\default.dds"
EndFunction

bool Function IsFemale(Actor akActor) global
	return akActor.GetActorBase().GetSex() == 1
EndFunction

string Function SlotNode(int aiSlot) global
	return "Face [Ovl" + aiSlot + "]"
EndFunction

string Function SlotTexture(Actor akActor, int aiSlot) global
	return NiOverride.GetNodeOverrideString(akActor, IsFemale(akActor), SlotNode(aiSlot), 9, 0)
EndFunction

bool Function SlotIsFree(Actor akActor, int aiSlot) global
	string tex = SlotTexture(akActor, aiSlot)
	return tex == "" || StringUtil.Find(tex, "default.dds") >= 0
EndFunction

int Function FindOwnSlot(Actor akActor) global
	int i = NiOverride.GetNumFaceOverlays() - 1
	while i >= 0
		if IsOwnTexture(SlotTexture(akActor, i))
			return i
		endif
		i -= 1
	endwhile
	return -1
EndFunction

int Function FindSlot(Actor akActor) global
	int own = FindOwnSlot(akActor)
	if own >= 0
		return own
	endif
	int i = NiOverride.GetNumFaceOverlays() - 1
	while i >= 0
		if SlotIsFree(akActor, i)
			return i
		endif
		i -= 1
	endwhile
	return -1
EndFunction

int Function Apply(Actor akActor, string asTexture) global
	if !akActor || NiOverride.GetNumFaceOverlays() <= 0
		return -1
	endif
	int slot = FindSlot(akActor)
	if slot < 0
		return -1
	endif
	bool female = IsFemale(akActor)
	string node = SlotNode(slot)
	NiOverride.AddNodeOverrideString(akActor, female, node, 9, 0, asTexture, true)
	NiOverride.AddNodeOverrideInt(akActor, female, node, 7, -1, 0xFFFFFF, true)
	NiOverride.AddNodeOverrideFloat(akActor, female, node, 8, -1, 1.0, true)
	return slot
EndFunction

bool Function Clear(Actor akActor) global
	if !akActor
		return false
	endif
	int slot = FindOwnSlot(akActor)
	if slot < 0
		return false
	endif
	bool female = IsFemale(akActor)
	string node = SlotNode(slot)
	NiOverride.AddNodeOverrideString(akActor, female, node, 9, 0, DefaultTexture(), false)
	NiOverride.RemoveAllNodeNameOverrides(akActor, female, node)
	return true
EndFunction
