"""Source contract for current-weapon selection's complete Host/Runtime/UI entry.
The behavioral native-weapon test lives in capacity_admission.cpp.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[3]
def source(path):
    return (root/path).read_text(encoding="utf-8-sig")
flag=source("source/shared/source/feature_availability.h")
settings=source("source/assist/source/additional_settings.h")
ui=source("source/assist/source/tactical_controls.h")
layout=source("source/suite/source/tactical_console_layout.h")
host=source("source/assist/source/main.cpp")
actions=source("source/assist/source/hotkey_host.h")
runtime=source("source/assist/source/type_select_bridge.h")
expectations={
    "enabled": "inline constexpr bool VariantSelection=true;" in flag,
    "saved shortcut": "variantKey='T'|(2<<8)" in settings,
    "editable input": 'extraControls[VariantKey]=key(724,extra.variantKey);' in ui,
    "visible caption": 'label(L"按当前武器选择同类单位",20,306);' in ui,
    "dedicated UI owner": '{724,categories::Control,L"选兵与编组",L"按当前武器选择同类单位",true,{724}}' in layout,
    "registered shortcut": "registerKey(1700,WORD(extra.variantKey))" in host,
    "host activation": "sendSelection(-2,global)" in actions,
    "runtime handler": "static int selectVariant(uint32_t house,bool global)" in runtime,
    "native normal-weapon resolver": "normalVariantWeapon(p,type)" in runtime,
    "no disabled placeholder": "static int selectVariant(uint32_t,bool){return -12;}" not in runtime,
}
for name,passed in expectations.items():
    print(("PASS " if passed else "FAIL ")+name)
if not all(expectations.values()):
    raise SystemExit(1)
print(f"PASS {len(expectations)} variant selection entry contracts")
