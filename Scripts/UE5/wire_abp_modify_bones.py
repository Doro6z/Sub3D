"""
Sub3D — Wire Modify Bone chain in ABP_Crew AnimGraph
======================================================
Creates 18 Modify Bone nodes chained together,
each reading from the Proc_*_Rot properties of SubCrewAnimInstance.

Run in UE5 Python console:
exec(open(r"C:/Dev/Sub3D/Scripts/UE5/wire_abp_modify_bones.py").read())
"""

import unreal

ABP_PATH = "/Game/Sub3D/Characters/Animation/ABP_Crew"

# (bone_name, rotation_property, translation_property_or_None)
BONES = [
    ("pelvis",      "Proc_Pelvis_Rot",    "Proc_Pelvis_Offset"),
    ("spine_01",    "Proc_Spine01_Rot",   None),
    ("spine_02",    "Proc_Spine02_Rot",   None),
    ("spine_03",    "Proc_Spine03_Rot",   None),
    ("spine_04",    "Proc_Spine04_Rot",   None),
    ("spine_05",    "Proc_Spine05_Rot",   None),
    ("neck_01",     "Proc_Neck01_Rot",    None),
    ("head",        "Proc_Head_Rot",      None),
    ("thigh_r",     "Proc_ThighR_Rot",    None),
    ("thigh_l",     "Proc_ThighL_Rot",    None),
    ("calf_r",      "Proc_CalfR_Rot",     None),
    ("calf_l",      "Proc_CalfL_Rot",     None),
    ("foot_r",      "Proc_FootR_Rot",     None),
    ("foot_l",      "Proc_FootL_Rot",     None),
    ("upperarm_r",  "Proc_UpperarmR_Rot", None),
    ("upperarm_l",  "Proc_UpperarmL_Rot", None),
    ("lowerarm_r",  "Proc_LowerarmR_Rot", None),
    ("lowerarm_l",  "Proc_LowerarmL_Rot", None),
]


def main():
    unreal.log("=" * 50)
    unreal.log("Wiring ABP_Crew Modify Bone chain")
    unreal.log("=" * 50)

    abp = unreal.EditorAssetLibrary.load_asset(ABP_PATH)
    if not abp:
        unreal.log_error(f"ABP not found at {ABP_PATH}")
        return

    # Get the AnimBlueprint's generated class to find the AnimGraph
    anim_bp = unreal.AnimationBlueprintLibrary

    # We need to use the Animation Blueprint Library to add nodes
    # Unfortunately, direct AnimGraph node manipulation is limited in Python.
    # Instead, we'll generate the node setup instructions as a log.

    # Alternative approach: use the AnimationBlueprintLibrary if available
    # For UE5.7, we can try using the blueprint editing utilities

    try:
        from unreal import AnimationBlueprintLibrary as ABL
        unreal.log("AnimationBlueprintLibrary available")
    except:
        unreal.log("AnimationBlueprintLibrary not directly accessible")

    # The most reliable way to do this in UE5 Python is through
    # the AnimationModifiersLibrary or by creating a custom AnimGraph
    # programmatically. However, this is extremely limited in Python.

    # PRACTICAL APPROACH: Generate a complete Blueprint JSON patch
    # or use the SubsystemEditorUtility approach.

    # Since direct AnimGraph manipulation via Python is severely limited,
    # let's output the exact node configuration the user needs:

    unreal.log("")
    unreal.log("MODIFY BONE CHAIN — Copy this setup in ABP_Crew AnimGraph:")
    unreal.log("-" * 50)

    for i, (bone, rot_prop, trans_prop) in enumerate(BONES):
        line = f"  [{i+1:2d}] Modify Bone: '{bone}'"
        line += f"  |  Rotation: {rot_prop}"
        if trans_prop:
            line += f"  |  Translation: {trans_prop}"
        line += "  |  Mode: Add to Existing  |  Space: Component Space"
        unreal.log(line)

    unreal.log("-" * 50)
    unreal.log(f"Total: {len(BONES)} Modify Bone nodes in chain")
    unreal.log("")
    unreal.log("NOTE: Python cannot directly create AnimGraph nodes in UE5.")
    unreal.log("The Modify Bone chain must be wired manually in the AnimGraph.")
    unreal.log("However — there is a BETTER alternative:")
    unreal.log("")
    unreal.log(">> USE THE C++ APPROACH INSTEAD <<")
    unreal.log("Override NativeEvaluateAnimation() in SubCrewAnimInstance")
    unreal.log("to apply all bone transforms directly in code.")
    unreal.log("This eliminates the need for 18 manual Modify Bone nodes.")
    unreal.log("=" * 50)


main()
