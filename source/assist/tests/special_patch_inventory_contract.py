"""Keep special patch admission explicit and document exact no-code alternatives."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")

# PlacementEvent is an ordinary native Place-event route. It must retire old
# adapters and must never silently fall back to the two patch implementations.
placement = read("assist/source/placement_bridge.h")
start = placement.index("if((sh->flags&BuildAnywhere)&&sh->placementImplementation==2)")
end = placement.index("}else if", start)
event_branch = placement[start:end]
assert "restoreRiskSite(" in event_branch and "directGroupRestore(" in event_branch
assert "installRiskSite(" not in event_branch and "directGroupEnable(" not in event_branch
assert "if(op==4)return 0;" in placement
assert "riskC4Active=false;" in placement

# Direct map reveal is five instruction sites; direct placement is two.
# The one-slot map-visibility DR is display-only and not a reveal substitute.
direct = read("shared/source/direct_patch_policy.h")
assert len(re.findall(r"\{0x[0-9A-Fa-f]+,Group::Map,", direct)) == 5
assert len(re.findall(r"\{0x[0-9A-Fa-f]+,Group::Placement,", direct)) == 2
map_policy = read("assist/source/map_visibility_parameter_policy.h")
assert "Address=0x6D88FC" in map_policy and "flags&~0x40u" in map_policy
controls = read("assist/source/tactical_controls.h")
assert "不等价于真正全图揭示、雷达或侦察卫星" in controls

# Native aircraft event mode and native technology parameter mode are
# no-instruction alternatives, with different semantics and DR costs.
building = read("assist/source/building_event_settings.h")
assert "PlacementEvent=2,TechData=3,TechParameters=4" in building
tech = read("assist/source/tech_ui_breakpoint_bridge.h")
assert "FullGroup=1u" in tech and "mapvisibility::Address" in tech
tech_ui = read("assist/source/tactical_controls.h")
assert "参数拦截，不改指令／规则" in tech_ui
assert "规则数据直接放宽建造规则，同时影响所有玩家和 AI" in tech_ui

# Native sender adaptation is only requested for multiplayer sender mode,
# with a valid current house and supported protocol.
network = read("assist/source/network_sender_bridge.h")
tick = network.split("static void networkSenderTick(bool requested)", 1)[1]
assert "requested=requested&&sh&&controlmode::sender" in tick
assert "eventdelivery::isMultiplayer" in tick and "networksender::protocol" in tick
policy = read("assist/source/network_sender_policy.h")
assert "constexpr unsigned SiteCount=sizeof(Sites)/sizeof(Sites[0]);" in policy
assert len(re.findall(r"\{(?:Sender|UncompressedHouse|Compressed|CompressedHouse|CacheLookup|FrameGroup|UncompressedLimit),", policy)) == 7

# CRC shadow is explicitly opted in, requires experimental reveal and MP;
# no background data-copy fallback is allowed to masquerade as equivalent.
crc = read("assist/source/crc_shadow_bridge.h")
assert "const bool requested=sh&&sh->crcMapShadow&&!sh->stop&&!fault;" in crc
assert "!(sh->flags&ExperimentalReveal)||sh->renderReveal||!eventdelivery::isMultiplayer" in crc
crc_policy = read("assist/source/crc_shadow_policy.h")
assert "SiteCount" in crc_policy

# Precise frame hook is diagnostic-only. Ordinary consumers retain the
# message-based fallback and must not silently install this patch.
frame = read("game_runtime/source/frame_signal_runtime.h")
assert "const bool deselected=channel&&!(channel->enabled&framesignal::ExplicitDiagnostic);" in frame
assert "if(!(channel->enabled&framesignal::ExplicitDiagnostic)){channel->state=framesignal::MessageFallback;" in frame

# Tooltip callbacks are data-pointer slots, not instruction detours.
tooltip = read("game_runtime/source/native_tooltip_runtime.h")
assert "Slot=0x7F74D8" in tooltip and "LayoutSlot=0x7F74C8" in tooltip
assert "buildTimeWanted||cursorWanted" in tooltip

# The failed SHP material experiment must not be reintroduced as a claimed
# no-code equivalent for the still-explicit shroud/fog draw patches.
shroud = read("assist/source/shroud_policy.h")
assert "did not produce a usable display" in shroud
patch_policy = read("shared/source/experimental_patch_policy.h")
assert "ShroudDraw" in patch_policy and "FogDraw" in patch_policy

print("PASS: special patch inventory, explicit gates, no-code alternatives and non-equivalence boundaries")
