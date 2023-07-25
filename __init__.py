bl_info = {
    "name": "Blood Money Tools",
    "description": "Tools for Blood Money",
    "version": (1, 0, 0),
    "blender": (3, 4, 0),
    "doc_url": "",
    "tracker_url": "https://github.com/glacier-modding/io_scene_blood_money/issues",
    "category": "Import-Export",
}

import bpy
import bpy_extras
from . import bmexport
import numpy

bm = bmexport.BloodMoney()

class OutputPathPreferences(bpy.types.AddonPreferences):
    bl_idname = __name__

    output_path: bpy.props.StringProperty(
        name="Output Path",
        subtype='DIR_PATH',
        default="",
        description="Output Path to export Blood Money models to"
    )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "output_path")

class ImportZip(bpy.types.Operator, bpy_extras.io_utils.ImportHelper):
    bl_idname = "import.zip"
    bl_label = "Import Blood Money Zip"
    filename_ext = ".zip"

    filter_glob: bpy.props.StringProperty(
        default="*.zip",
        options={'HIDDEN'},
    )

    files: bpy.props.CollectionProperty(
        name="File Path",
        type=bpy.types.OperatorFileListElement,
    )

    def draw(self, context):
        if ".zip" not in self.filepath.lower():
            return

        layout = self.layout
        layout.row(align=True)
        
    def execute(self, context):
        print(self.filepath)        
        output_path = context.preferences.addons[__name__].preferences.output_path
        print(output_path)
        bm.load(self.filepath, context, output_path)
        return {'FINISHED'}

class CUSTOM_OT_LoadModel(bpy.types.Operator):
    bl_idname = "custom.load_model"
    bl_label = "Load Model"
    bl_description = "Load Blood Money Model"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        try:
            item = context.scene.custom[context.scene.custom_index]
            #print("Name:", item.name, "-", "ID:", item.model_id)
        except IndexError:
            return{'FINISHED'}
        sub_meshes_list = []
        obj = bm.import_model_in_blender(context, bm.scene_json["Scene"][item.scene_id], item.name, None, sub_meshes_list)
        obj.matrix_local = (
            (-0.01, 0.0, 0.0, 0.0),
            (0.0, 0.0, 0.01, 0.0),
            (0.0, -0.01, 0.0, 0.0),
            (0.0, 0.0, 0.0, 1.0)
        )
        return{'FINISHED'}

class CUSTOM_OT_LoadMap(bpy.types.Operator):
    bl_idname = "custom.load_map"
    bl_label = "Load Map"
    bl_description = "Load Blood Money Map"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        bm.import_map(context)
        return{'FINISHED'}

class ImportSettings(bpy.types.PropertyGroup):
    my_float: bpy.props.FloatProperty(
        name='Float',
        default=0.0 
    )

    exclude_dynamic: bpy.props.BoolProperty(
        name="Exclude Dynamic Items",
        description="Excludes dynamic items from the map (weapons, interactables, etc)",
        default=True
    )

    exclude_collision: bpy.props.BoolProperty(
        name="Exclude Collision Meshes",
        description="Excludes _Glacier collision meshes",
        default=True
    )

    exclude_overlay: bpy.props.BoolProperty(
        name="Exclude Map Overlay Meshes",
        description="Excludes map overlay meshes",
        default=True
    )

    exclude_fog: bpy.props.BoolProperty(
        name="Exclude Fog Meshes",
        description="Excludes fog plane meshes",
        default=True
    )

    exclude_dust: bpy.props.BoolProperty(
        name="Exclude Dust Meshes",
        description="Excludes dust plane meshes",
        default=True
    )

    exclude_glow: bpy.props.BoolProperty(
        name="Exclude Glow Meshes",
        description="Excludes glow meshes",
        default=True
    )

    exclude_cover: bpy.props.BoolProperty(
        name="Exclude Cover Meshes",
        description="Excludes cover meshes",
        default=True
    )

    exclude_bbox: bpy.props.BoolProperty(
        name="Exclude Bounding Box Meshes",
        description="Excludes bounding box meshes",
        default=True
    )

    exclude_shadow: bpy.props.BoolProperty(
        name="Exclude Shadow Meshes",
        description="Excludes shadow meshes",
        default=True
    )

    exclude_textures: bpy.props.BoolProperty(
        name="Exclude Invalid Textures",
        description="Excludes invalid textures",
        default=True
    )

class CUSTOM_UL_Items(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        self.use_filter_show = True
        split = layout.row()
        custom_icon = "COLOR"
        split.prop(item, "name", emboss=False, text="")

    def invoke(self, context, event):
        pass   

class CUSTOM_PT_BloodMoneyPanel(bpy.types.Panel):
    bl_idname = 'BLOODMONEY_PT_settingsPanel'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Blood Money'
    bl_label = 'Blood Money'

    def draw(self, context):
        layout = self.layout
        scn = bpy.context.scene

        rows = 10
        row = layout.row()
        row.template_list("CUSTOM_UL_Items", "", scn, "custom", scn, "custom_index", rows=rows)

        row = layout.row()
        row = layout.row(align=True)
        row.operator("custom.load_model")
        row = layout.row(align=True)
        row.operator("custom.load_map")        
        layout.label(text="General Options:")
        row = layout.row(align=True)
        row.prop(context.preferences.addons[__name__].preferences, "output_path")

class CUSTOM_PT_BloodMoneyImportOptions(bpy.types.Panel):
    bl_idname = 'BLOODMONEY_PT_importOptions'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = 'Map Import Options'
    bl_parent_id = 'BLOODMONEY_PT_settingsPanel'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False

        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_dynamic")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_collision")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_bbox")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_overlay")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_fog")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_dust")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_glow")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_cover")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_shadow")
        row = layout.row(align=True)
        row.prop(context.scene.import_settings, "exclude_textures")

class CUSTOM_ModelData(bpy.types.PropertyGroup):
    model_id: bpy.props.IntProperty()
    scene_id: bpy.props.IntProperty()

classes = (
    OutputPathPreferences,
    ImportZip,
    CUSTOM_OT_LoadModel,
    CUSTOM_OT_LoadMap,
    ImportSettings,
    CUSTOM_UL_Items,
    CUSTOM_PT_BloodMoneyPanel,
    CUSTOM_PT_BloodMoneyImportOptions,
    CUSTOM_ModelData,
)

def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    bpy.types.Scene.custom = bpy.props.CollectionProperty(type=CUSTOM_ModelData)
    bpy.types.Scene.custom_index = bpy.props.IntProperty()
    bpy.types.Scene.import_settings = bpy.props.PointerProperty(type=ImportSettings)


def unregister():
    from bpy.utils import unregister_class
    for cls in reversed(classes):
        unregister_class(cls)

    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    del bpy.types.Scene.custom
    del bpy.types.Scene.custom_index
    del bpy.types.Scene.import_settings

def menu_func_import(self, context):
    self.layout.operator(ImportZip.bl_idname, text="Blood Money Scene Zip (.zip)")

if __name__ == "__main__":
    register()