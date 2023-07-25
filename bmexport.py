import bpy
import os
import sys
import zipfile
import struct
import ctypes
import json
import mathutils
import pathlib
import time

class BloodMoney:
    def __init__(self):
        self.dir = os.path.abspath(os.path.dirname(__file__))
        if not os.environ['PATH'].startswith(self.dir):
            os.environ['PATH'] = self.dir + os.pathsep + os.environ['PATH']
        self.bmexport = ctypes.CDLL(os.path.abspath(os.path.join(os.path.dirname(__file__), "bmexport.dll")), winmode=0)
        self.glaciertex = ctypes.CDLL(os.path.abspath(os.path.join(os.path.dirname(__file__), "glaciertex.dll")), winmode=0)
        self.bmexport.LoadData.argtypes = (ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32,)
        self.bmexport.LoadData.restype = ctypes.c_char_p
        self.bmexport.GetMaterialJson.argtypes = ()
        self.bmexport.GetMaterialJson.restype = ctypes.c_char_p
        self.bmexport.GetSceneJson.argtypes = ()
        self.bmexport.GetSceneJson.restype = ctypes.c_char_p
        self.bmexport.WriteObj.argtypes = (ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,)
        self.bmexport.WriteObj.restype = ctypes.c_char_p
        self.glaciertex.LoadTex.argtypes = (ctypes.c_void_p, ctypes.c_uint32,)
        self.glaciertex.LoadTex.restype = ctypes.c_char_p
        self.glaciertex.WriteTga.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32,)
        self.glaciertex.WriteTga.restype = ctypes.c_int
        self.glaciertex.FreeJson.argtypes = ()
        self.glaciertex.FreeJson.restype = ctypes.c_int
        
    def load(self, filepath, context, output_path):
        self.map = pathlib.Path(filepath).name
        self.output_path = output_path
        if self.output_path == "":
            self.output_path = self.dir
        self.map = self.map[:self.map.find(".")]
        if not os.path.exists(self.output_path + os.sep + self.map):
            os.system("mkdir \"" + self.output_path + os.sep + self.map + "\"")
        zip_data = {"BUF": None, "GMS": None, "MAT": None, "PRM": None, "PRP": None, "TEX": None}
        with zipfile.ZipFile(filepath) as scene:
            for file in scene.namelist():
                for data in zip_data:
                    if file.upper().endswith("." + data):
                        with scene.open(file) as f:
                            zip_data[data] = f.read()
        for data in zip_data:
            if zip_data[data] == None:
                print("Error: Missing file type " + data)
                return
        char_arrays = {}
        for data in zip_data:
            char_arrays[data] = ctypes.c_char * len(zip_data[data])
        buffer = ctypes.c_char_p(zip_data["TEX"])
        tex_json = str(self.glaciertex.LoadTex(buffer, len(zip_data["TEX"])))
        tex_json = tex_json[tex_json.find("{"):tex_json.find("}") + 1].encode()
        self.tex_json = json.loads(tex_json)
        self.glaciertex.FreeJson()
        del buffer
        char_arrays["TEXJSON"] = ctypes.c_char * len(tex_json)
        json_string = self.bmexport.LoadData(
            char_arrays["BUF"].from_buffer_copy(zip_data["BUF"]),
            len(zip_data["BUF"]),
            char_arrays["GMS"].from_buffer_copy(zip_data["GMS"]),
            len(zip_data["GMS"]),
            char_arrays["MAT"].from_buffer_copy(zip_data["MAT"]),
            len(zip_data["MAT"]),
            char_arrays["PRM"].from_buffer_copy(zip_data["PRM"]),
            len(zip_data["PRM"]),
            char_arrays["PRP"].from_buffer_copy(zip_data["PRP"]),
            len(zip_data["PRP"]),
            char_arrays["TEX"].from_buffer_copy(zip_data["TEX"]),
            len(zip_data["TEX"]),
            char_arrays["TEXJSON"].from_buffer_copy(tex_json),
            len(tex_json)
        )
        del char_arrays
        del tex_json
        json_string = self.bmexport.GetSceneJson()
        self.scene_json = json.loads(json_string)
        for model in self.scene_json["Scene"]:
            if "Meshes" in model:
                scn = context.scene
                item = scn.custom.add()
                item.name = model["Parent"] + " / " + model["Name"] + " (" + str(model["Model Id"]) + ")"
                item.model_id = model["Model Id"]
                item.scene_id = model["Index"]
                scn.custom_index = item.model_id
        with open(self.output_path + os.sep + self.map + os.sep + "scene.json", "w") as j:
            json.dump(self.scene_json, j, indent=4)        
        json_string = self.bmexport.GetMaterialJson()
        self.mat_json = json.loads(json_string)
        with open(self.output_path + os.sep + self.map + os.sep + "mat.json", "w") as j:
            json.dump(self.mat_json, j, indent=4)
        with open(self.output_path + os.sep + self.map + os.sep + "tex.json", "w") as j:
            json.dump(self.tex_json, j, indent=4)
        self.materials = {}
        self.imported = {}
        self.imported["Textures"] = {}
        self.root_node = None

    def write_model(self, output_path, model_id, scene_id, sub_meshes_list):
        filepath = self.output_path.encode() + os.sep.encode() + self.map.encode() + os.sep.encode()
        sub_meshes = (ctypes.c_uint32 * len(sub_meshes_list))(*sub_meshes_list)
        self.bmexport.WriteObj(ctypes.c_char_p(filepath), model_id, sub_meshes, len(sub_meshes))
        for mesh in self.scene_json["Scene"][scene_id]["Meshes"]:
            if mesh["Material Id"] not in self.materials:
                if mesh["Diffuse Id"] > -1:
                    if mesh["Diffuse Id"] not in self.imported["Textures"]:
                        if "Diffuse Path" in mesh and "Diffuse File" in mesh:
                            texture_dir = filepath + mesh["Diffuse Path"].encode()
                            texture_file = mesh["Diffuse File"].encode()
                            self.glaciertex.WriteTga(texture_dir, texture_file, mesh["Diffuse Id"])
                            self.imported["Textures"][mesh["Diffuse Id"]] = True
                if mesh["Normal Id"] > -1:                            
                    if mesh["Normal Id"] not in self.imported["Textures"]:
                        if "Normal Path" in mesh and "Normal File" in mesh:
                            texture_dir = filepath + mesh["Normal Path"].encode()
                            texture_file = mesh["Normal File"].encode()
                            self.glaciertex.WriteTga(texture_dir, texture_file, mesh["Normal Id"])
                            self.imported["Textures"][mesh["Normal Id"]] = True
                if mesh["Specular Id"] > -1:
                    if mesh["Specular Id"] not in self.imported["Textures"]:
                        if "Specular Path" in mesh and "Specular File" in mesh:
                            texture_dir = filepath + mesh["Specular Path"].encode()
                            texture_file = mesh["Specular File"].encode()
                            self.glaciertex.WriteTga(texture_dir, texture_file, mesh["Specular Id"])
                            self.imported["Textures"][mesh["Specular Id"]] = True
        return filepath + b"Models" + os.sep.encode() + str(model_id).encode() + b".obj"
    
    def import_model_in_blender(self, context, model, name, parent, sub_meshes_list):
        filepath = self.write_model("", model["Model Id"], model["Index"], sub_meshes_list)
        bpy.ops.wm.obj_import(
            filepath = filepath,
            forward_axis = 'Z',
            up_axis = 'Y'
        )
        if len(context.selected_objects) > 0:
            context.view_layer.objects.active = context.selected_objects[0]
            obj = context.object
            obj.name = name
            if parent != None:
                if parent in bpy.data.objects:
                    obj.parent = bpy.data.objects[parent]
                    obj.matrix_parent_inverse = obj.parent.matrix_world.inverted()
            obj.matrix_local = self.get_matrix(model["Index"])
            if self.root_node == None:
                self.root_node = obj
            for slot in obj.material_slots:
                if ".0" in slot.material.name:
                    old_material = slot.material
                    slot.material = bpy.data.materials[slot.material.name[:slot.material.name.find(".")]]
                    bpy.data.materials.remove(old_material, do_unlink=True)
            #bpy.ops.mesh.separate(type='MATERIAL')
            if obj != None:
                for mesh in model["Meshes"]:
                    material = self.mat_json["Entries"][mesh["Material Id"] - 1]["Instance"][0]["Binder"][0]["Render State"][0]
                    if "Blend Enabled" in material and "Blend Mode" in material and "Alpha Reference" in material and "Culling Mode" in material:
                        if material["Blend Enabled"] == 1 and (material["Alpha Reference"] != 254 or material["Opacity"] != 1.0 or (material["Blend Mode"] != "TRANS" and material["Alpha Reference"] == 254) or material["Culling Mode"] == "TwoSided"):
                            for slot in obj.material_slots:
                                if slot.material.name.startswith(str(mesh["Material Id"]) + "-") and slot.material.use_nodes:                                    
                                    for n in slot.material.node_tree.nodes:
                                        if n.type == "BSDF_PRINCIPLED" and "Base Color" in n.inputs:
                                            for l in n.inputs["Base Color"].links:
                                                if l.from_node.type == "TEX_IMAGE":
                                                    if material["Blend Mode"] == "TRANS" and material["Opacity"] == 1.0:
                                                        slot.material.blend_method = 'CLIP'
                                                    else:
                                                        slot.material.blend_method = 'BLEND'
                                                    if material["Opacity"] == 1.0:
                                                        slot.material.node_tree.links.new(l.from_node.outputs['Alpha'], n.inputs['Alpha'])
                                                    else:
                                                        n.inputs['Alpha'].default_value = material["Opacity"]
            return obj
        else:
            return None
    
    def import_models(self, context, index, parent, no_children_mesh):
        self.update_progress()
        model = self.scene_json["Scene"][index]
        name = str(model["Index"]) + " - " + model["Name"]
        if len(name) > 63:
            name = name[:63]
        if context.scene.import_settings.exclude_dynamic:
            if "includes." in name or \
                "equipment." in name:
                no_children_mesh = True
        sub_meshes_list = []
        exclude_model = False
        is_model = False
        if "Meshes" in model and not no_children_mesh:
            is_model = True
            model_name = name.lower()
            model_type = model["Type"].lower()
            if context.scene.import_settings.exclude_bbox:
                if model_type == "zbound":
                    exclude_model = True
            if context.scene.import_settings.exclude_fog:
                if "fog" in model_name:
                    exclude_model = True
            if context.scene.import_settings.exclude_dust:
                if "dust_" in model_name:
                    exclude_model = True
            if context.scene.import_settings.exclude_glow:
                if "glow" in model_name:
                    exclude_model = True
            if context.scene.import_settings.exclude_cover:
                if "_cover_" in model_name:
                    exclude_model = True
            if context.scene.import_settings.exclude_shadow:
                if model_type == "zshadowmeshobj":
                    exclude_model = True
            if not exclude_model:
                model_index = -1
                for mesh in model["Meshes"]:
                    model_index += 1
                    material_path = mesh["Material Path"].lower()
                    material_file = mesh["Material File"].lower()
                    if context.scene.import_settings.exclude_textures:
                        if mesh["Diffuse Id"] <= 0:
                            sub_meshes_list.append(model_index)
                            continue
                    if context.scene.import_settings.exclude_collision:
                        if "_glacier" in material_path or \
                            "_test" in material_path:
                            sub_meshes_list.append(model_index)
                            continue
                        if "Diffuse Path" in mesh:
                            if "_test" in mesh["Diffuse Path"].lower():
                                sub_meshes_list.append(model_index)
                                continue
                    if context.scene.import_settings.exclude_dynamic:
                        if "dynamic" in material_path:
                            sub_meshes_list.append(model_index)
                            continue
                    if context.scene.import_settings.exclude_collision:
                        if "_glacier" in material_path or \
                            "_test" in material_path:
                            sub_meshes_list.append(model_index)
                            continue
                    if context.scene.import_settings.exclude_overlay:
                        if "screens" in material_path:
                            sub_meshes_list.append(model_index)
                            continue
                    if context.scene.import_settings.exclude_fog:
                        if "fog" in material_path or \
                            "fog" in material_file:
                            sub_meshes_list.append(model_index)
                            continue
                    if context.scene.import_settings.exclude_dust:
                        if "dust_" in material_path or \
                            "dust_" in material_file:
                            sub_meshes_list.append(model_index)
                            continue
                    if context.scene.import_settings.exclude_glow:
                        if "glow" in material_path or \
                            "glow" in material_file:
                            sub_meshes_list.append(model_index)
                            continue
                if len(sub_meshes_list) == len(model["Meshes"]):
                     exclude_model = True
        if is_model and not exclude_model:
            obj = self.import_model_in_blender(context, model, name, parent, sub_meshes_list)
        else:
            obj = bpy.data.objects.new(name, None)
            context.collection.objects.link(obj)
            if parent != None:
                if parent in bpy.data.objects:
                    obj.parent = bpy.data.objects[parent]
                    obj.matrix_parent_inverse = obj.parent.matrix_world.inverted()
                    obj.matrix_local = self.get_matrix(model["Index"])
            obj.matrix_local = self.get_matrix(model["Index"])
            if self.root_node == None:
                self.root_node = obj
        if "Children" in model:
            for child in model["Children"]:
                self.import_models(context, child["Index"], name, no_children_mesh)

    def import_map(self, context):
        from bpy.ops import _BPyOpsSubModOp
        view_layer_update = _BPyOpsSubModOp._view_layer_update
        def dummy_view_layer_update(context):
            pass
        try:
            _BPyOpsSubModOp._view_layer_update = dummy_view_layer_update
            time_start = time.time()
            self.imported = {}
            self.imported["Textures"] = {}
            self.root_node = None
            root_index = -1
            for model in self.scene_json["Scene"]:
                if model["Parent"] == None:
                    root_index = model["Index"]
                    break
            if root_index != -1:
                self.progress = 0
                self.progress_step = int(len(self.scene_json["Scene"]) / 100)
                bpy.context.window_manager.progress_begin(0, 100)
                self.import_models(context, root_index, None, False)
                self.root_node.matrix_local = (
                    (-0.01, 0.0, 0.0, 0.0),
                    (0.0, 0.0, 0.01, 0.0),
                    (0.0, -0.01, 0.0, 0.0),
                    (0.0, 0.0, 0.0, 1.0)
                )
                bpy.context.window_manager.progress_end()
                print("Loaded Map " + self.map + " in " + str(time.time() - time_start) + " seconds")
            else:
                print("Error: The root scene node for the map could not be found.")
        finally:
            _BPyOpsSubModOp._view_layer_update = view_layer_update

    def get_matrix(self, scene_id):
        return (
            (
                self.scene_json["Scene"][scene_id]["Rotation"]["Z Axis"]["X"],
                self.scene_json["Scene"][scene_id]["Rotation"]["Z Axis"]["Y"],
                self.scene_json["Scene"][scene_id]["Rotation"]["Z Axis"]["Z"],
                0.0
            ),
            (
                self.scene_json["Scene"][scene_id]["Rotation"]["Y Axis"]["X"],
                self.scene_json["Scene"][scene_id]["Rotation"]["Y Axis"]["Y"],
                self.scene_json["Scene"][scene_id]["Rotation"]["Y Axis"]["Z"],
                0.0
            ),
            (
                self.scene_json["Scene"][scene_id]["Rotation"]["X Axis"]["X"],
                self.scene_json["Scene"][scene_id]["Rotation"]["X Axis"]["Y"],
                self.scene_json["Scene"][scene_id]["Rotation"]["X Axis"]["Z"],
                0.0
            ),
            (
                self.scene_json["Scene"][scene_id]["Position"]["X"],
                self.scene_json["Scene"][scene_id]["Position"]["Y"],
                self.scene_json["Scene"][scene_id]["Position"]["Z"],
                1.0
            )
        )

    def update_progress(self):
        self.progress += 1
        if self.progress % self.progress_step == 0:
            bpy.context.window_manager.progress_update(self.progress / self.progress_step)