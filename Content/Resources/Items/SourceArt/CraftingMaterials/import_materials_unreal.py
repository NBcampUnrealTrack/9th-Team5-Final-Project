"""Import the four authored material props and connect only Icon/WorldMesh on their item definitions."""
from pathlib import Path
import json
import sys
import unreal

ROOT = Path(__file__).resolve().parent
PROJECT = next(p for p in ROOT.parents if any(p.glob('*.uproject')))
ART = '/Game/Resources/Items'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MESHES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()

# Linear color, metallic, roughness, variation, relief. Match the existing item material pipeline.
PALETTE = {
    'Steel': ((.47, .49, .50), .92, .34, .07, .018),
    'DarkSteel': ((.025, .030, .035), .8, .48, .12, .02),
    'Gunmetal': ((.075, .085, .090), .85, .45, .09, .025),
    'BlueBand': ((.035, .075, .12), .65, .55, .2, .03),
    'Aramid': ((.42, .28, .08), 0, .86, .20, .28),
    'GraphiteFabric': ((.035, .040, .043), 0, .92, .2, .24),
    'Webbing': ((.055, .065, .035), 0, .9, .2, .26),
    'Thread': ((.55, .39, .14), 0, .9, .1, .10),
    'Cardboard': ((.18, .11, .05), 0, .98, .1, .1),
    'Rubber': ((.018, .021, .023), 0, .70, .1, .06),
    'Brass': ((.42, .26, .08), .85, .40, .08, .02),
    'Copper': ((.55, .17, .055), .92, .32, .07, .018),
    'SafetyYellow': ((.60, .37, .015), .1, .60, .18, .035),
    'Crystal': ((.55, .025, .015), .12, .22, 0, 0),
    'CrystalDark': ((.14, .006, .004), .12, .28, 0, 0),
}


def imported(source, folder, name, options=None, factory=None):
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(source), destination_path=folder, destination_name=name,
                           automated=True, replace_existing=True, save=False).items(): task.set_editor_property(key, value)
    if options: task.set_editor_property('options', options)
    if factory: task.set_editor_property('factory', factory)
    TOOLS.import_asset_tasks([task])
    result = unreal.load_asset(folder+'/'+name)
    assert result, (name, task.get_editor_property('imported_object_paths'))
    return result


def crystal_master():
    name = 'M_CM_Crystal'; folder = ART+'/Materials/CraftingMaterials'
    mat = unreal.load_asset(folder+'/'+name) if LIB.does_asset_exist(folder+'/'+name) else TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(mat)
    color = EDIT.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property('parameter_name', 'BaseColor')
    color.set_editor_property('default_value', unreal.LinearColor(.55, .025, .015, 1))
    EDIT.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    for parameter, default, prop in [('Roughness', .22, unreal.MaterialProperty.MP_ROUGHNESS), ('Metallic', .12, unreal.MaterialProperty.MP_METALLIC)]:
        node = EDIT.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 200 if parameter=='Roughness' else 350)
        node.set_editor_property('parameter_name', parameter); node.set_editor_property('default_value', default)
        EDIT.connect_material_property(node, '', prop)
    strength = EDIT.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 500)
    strength.set_editor_property('parameter_name', 'EmissiveStrength'); strength.set_editor_property('default_value', .45)
    product = EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 500)
    EDIT.connect_material_expressions(color, 'RGB', product, 'A'); EDIT.connect_material_expressions(strength, '', product, 'B')
    EDIT.connect_material_property(product, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    errors = EDIT.recompile_material(mat); assert not errors, list(errors)
    assert LIB.save_loaded_asset(mat, False)
    return mat


def materials():
    master = unreal.load_asset(ART+'/Materials/M_ItemSurface'); assert master
    crystal = crystal_master(); result = {}
    for slot, (rgb, metallic, roughness, variation, relief) in PALETTE.items():
        name = 'MI_CM_'+slot; folder = ART+'/Materials/CraftingMaterials'
        instance = unreal.load_asset(folder+'/'+name) if LIB.does_asset_exist(folder+'/'+name) else TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        EDIT.set_material_instance_parent(instance, crystal if slot.startswith('Crystal') else master)
        EDIT.set_material_instance_vector_parameter_value(instance, 'BaseColor', unreal.LinearColor(*rgb, 1))
        assert EDIT.get_material_instance_vector_parameter_value(instance, 'BaseColor') == unreal.LinearColor(*rgb, 1)
        for parameter, value in [('Metallic', metallic), ('Roughness', roughness), ('SurfaceVariation', variation), ('SurfaceRelief', relief)]:
            EDIT.set_material_instance_scalar_parameter_value(instance, parameter, value)
        if slot.startswith('Crystal'): EDIT.set_material_instance_scalar_parameter_value(instance, 'EmissiveStrength', .45 if slot=='Crystal' else .15)
        EDIT.update_material_instance(instance); assert LIB.save_loaded_asset(instance, False)
        result[slot] = instance
    result['Glass'] = unreal.load_asset(ART+'/Materials/M_ItemBottleGlass'); assert result['Glass']
    return result


def run():
    palette = materials()
    reports = []
    for spec in json.loads((ROOT/'mesh_manifest.json').read_text(encoding='utf-8')):
        name = spec['name']
        icon = imported(PROJECT/('Content/Resources/Items/Icon/T_Icon_'+name+'.png'), ART+'/Icon', 'T_Icon_'+name, factory=unreal.TextureFactory())
        for key, value in dict(lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
                               compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                               mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                               never_stream=True, max_texture_size=512, srgb=True).items(): icon.set_editor_property(key, value)
        assert LIB.save_loaded_asset(icon, False)
        options = unreal.FbxImportUI()
        for key, value in dict(import_mesh=True, import_as_skeletal=False, mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH,
                               automated_import_should_detect_type=False, import_materials=False, import_textures=False).items(): options.set_editor_property(key, value)
        data = options.get_editor_property('static_mesh_import_data')
        for key, value in dict(combine_meshes=True, auto_generate_collision=False, convert_scene=False, convert_scene_unit=False,
                               normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items(): data.set_editor_property(key, value)
        mesh = imported(ROOT/('Meshes/SM_Item_'+name+'.obj'), ART+'/Mesh/Materials', 'SM_Item_'+name, options, unreal.FbxFactory())
        slots = mesh.get_editor_property('static_materials')
        for index, slot in enumerate(slots):
            key = str(slot.get_editor_property('imported_material_slot_name'))
            if key not in palette: key = str(slot.get_editor_property('material_slot_name'))
            assert key in palette, (name, index, key)
            mesh.set_material(index, palette[key])
        reductions = unreal.StaticMeshReductionOptions(); reductions.set_editor_property('auto_compute_lod_screen_size', False)
        settings = []
        for amount, screen in [(1., 1.), (.5, .30), (.2, .10)]:
            setting = unreal.StaticMeshReductionSettings(); setting.set_editor_property('percent_triangles', amount); setting.set_editor_property('screen_size', screen); settings.append(setting)
        reductions.set_editor_property('reduction_settings', settings)
        assert MESHES.set_lods(mesh, reductions) == 3
        MESHES.remove_collisions(mesh); assert MESHES.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX) >= 0
        mesh.set_editor_property('light_map_resolution', 64)
        bounds = mesh.get_bounding_box()
        actual = [bounds.max.x-bounds.min.x, bounds.max.y-bounds.min.y, bounds.max.z-bounds.min.z]
        expected = [spec['bounds_cm']['max'][i]-spec['bounds_cm']['min'][i] for i in range(3)]
        assert all(abs(a-b) < .1 for a, b in zip(actual, expected)), (name, 'axis/scale mismatch', actual, expected)
        assert abs(bounds.min.z) < .1, (name, 'pivot must be at floor')
        assert LIB.save_loaded_asset(mesh, False)
        item = unreal.load_asset('/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_'+name); assert item
        preserved = {k:str(item.get_editor_property(k)) for k in ['item_id', 'display_name', 'description', 'item_type', 'rarity', 'max_stack_size']}
        item.set_editor_property('icon', icon); item.set_editor_property('world_mesh', mesh)
        assert LIB.save_loaded_asset(item, False)
        assert preserved == {k:str(item.get_editor_property(k)) for k in preserved}
        report = dict(name=name, icon=icon.get_path_name(), mesh=mesh.get_path_name(), item=item.get_path_name(),
                      material_slots=len(slots), lod_vertices=[MESHES.get_number_verts(mesh, i) for i in range(3)], size_cm=actual, preserved=preserved)
        reports.append(report); unreal.log('CRAFT_ART_IMPORTED '+json.dumps(report, ensure_ascii=False))
    (ROOT/'unreal_manifest.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('CRAFT_ART_IMPORTED PASS items=4 icons=4 meshes=4')


def verify():
    """Read back saved assets in a fresh editor process without modifying them."""
    for report in json.loads((ROOT/'unreal_manifest.json').read_text(encoding='utf-8')):
        item = unreal.load_asset(report['item'])
        icon = unreal.load_asset(report['icon'])
        mesh = unreal.load_asset(report['mesh'])
        assert item and isinstance(icon, unreal.Texture2D) and isinstance(mesh, unreal.StaticMesh)
        assert item.get_editor_property('icon') == icon
        assert item.get_editor_property('world_mesh') == mesh
        assert report['preserved'] == {key:str(item.get_editor_property(key)) for key in report['preserved']}
        assert icon.get_editor_property('lod_group') == unreal.TextureGroup.TEXTUREGROUP_UI
        assert icon.get_editor_property('max_texture_size') == 512
        assert MESHES.get_lod_count(mesh) == 3
        assert MESHES.get_simple_collision_count(mesh) > 0
        assert [MESHES.get_number_verts(mesh, i) for i in range(3)] == report['lod_vertices']
        assert all(slot.get_editor_property('material_interface') for slot in mesh.get_editor_property('static_materials'))
        unreal.log('CRAFT_ART_VERIFIED '+report['name'])
    unreal.log('CRAFT_ART_VERIFIED PASS items=4 icons=4 meshes=4 lods=12')


if __name__ == '__main__':
    if '--verify-only' in sys.argv: verify()
    else: run()
