"""Run inside Unreal Editor Python to import bag art and connect existing bag definitions."""
from pathlib import Path
import json
import unreal

ROOT=Path(__file__).resolve().parent
PROJECT=next(parent for parent in ROOT.parents if any(parent.glob('*.uproject')))
ICON_SOURCE=PROJECT/'Content/Resources/Items/Icon'
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.EditorAssetLibrary
# Commandlet sessions do not automatically create this editor subsystem.
MESHES=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()


def import_asset(source,folder,name,options=None,factory=None):
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(source))
    task.set_editor_property('destination_path',folder)
    task.set_editor_property('destination_name',name)
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',True)
    task.set_editor_property('save',False)
    if options:task.set_editor_property('options',options)
    if factory:task.set_editor_property('factory',factory)
    TOOLS.import_asset_tasks([task])
    obj=unreal.load_asset(folder+'/'+name)
    assert obj, (source,list(task.get_editor_property('imported_object_paths')))
    return obj


def texture(source,folder,name,kind):
    tex=import_asset(source,folder,name,factory=unreal.TextureFactory())
    if kind=='icon':
        tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property('never_stream',True)
        tex.set_editor_property('max_texture_size',512)
    elif kind=='normal':
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb',False)
        tex.set_editor_property('flip_green_channel',True)
    elif kind=='orm':
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property('srgb',False)
    else:
        tex.set_editor_property('srgb',True)
    assert LIB.save_loaded_asset(tex,False)
    return tex


def master_material(defaults):
    path='/Game/Resources/Items/Materials/Bags/M_BagFabric'
    master=unreal.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset('M_BagFabric','/Game/Resources/Items/Materials/Bags',unreal.Material,unreal.MaterialFactoryNew())
    editing=unreal.MaterialEditingLibrary
    editing.delete_all_material_expressions(master)
    for index,(parameter,image) in enumerate(defaults.items()):
        sample=editing.create_material_expression(master,unreal.MaterialExpressionTextureSampleParameter2D,-500,index*220)
        sample.set_editor_property('parameter_name',parameter)
        sample.set_editor_property('texture',image)
        if parameter=='Normal':
            sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            assert editing.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_NORMAL)
        elif parameter=='ORM':
            sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            for output,prop in [('R',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),('G',unreal.MaterialProperty.MP_ROUGHNESS),('B',unreal.MaterialProperty.MP_METALLIC)]:
                assert editing.connect_material_property(sample,output,prop)
        else:
            assert editing.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    errors=editing.recompile_material(master)
    assert not errors,list(errors)
    assert LIB.save_loaded_asset(master,False)
    return master


def import_bags():
    assets={}
    for index in range(1,5):
        bag='Bag%02d'%index
        icon=texture(ICON_SOURCE/('T_Icon_'+bag+'.png'),'/Game/Resources/Items/Icon','T_Icon_'+bag,'icon')
        maps={}
        for suffix,kind in [('BaseColor','color'),('Normal','normal'),('ORM','orm')]:
            name='T_'+bag+'_'+suffix
            maps[suffix]=texture(ROOT/'Textures'/(name+'.png'),'/Game/Resources/Items/Textures/Bags',name,kind)
        assets[bag]={'icon':icon,'textures':maps}
    master=master_material(assets['Bag01']['textures'])
    reports=[]
    for index in range(1,5):
        bag='Bag%02d'%index
        name='MI_'+bag
        path='/Game/Resources/Items/Materials/Bags/'+name
        instance=unreal.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(name,'/Game/Resources/Items/Materials/Bags',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance,master)
        for parameter,tex in assets[bag]['textures'].items():
            # UE 5.8's setter returns false even after a successful update; check the actual value.
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance,parameter,tex)
            assert unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance,parameter)==tex
        unreal.MaterialEditingLibrary.update_material_instance(instance)
        assert LIB.save_loaded_asset(instance,False)
        options=unreal.FbxImportUI()
        options.set_editor_property('import_mesh',True)
        options.set_editor_property('import_as_skeletal',False)
        options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
        options.set_editor_property('automated_import_should_detect_type',False)
        options.set_editor_property('import_materials',False)
        options.set_editor_property('import_textures',False)
        data=options.get_editor_property('static_mesh_import_data')
        data.set_editor_property('combine_meshes',True)
        data.set_editor_property('auto_generate_collision',False)
        data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
        mesh=import_asset(ROOT/'Meshes'/('SM_'+bag+'.fbx'),'/Game/Resources/Items/Mesh/Bags','SM_'+bag,options,unreal.FbxFactory())
        assert MESHES.get_number_materials(mesh)==1
        mesh.set_material(0,instance)
        reductions=unreal.StaticMeshReductionOptions()
        reductions.set_editor_property('auto_compute_lod_screen_size',False)
        settings=[]
        for percent,screen in [(1.0,1.0),(0.5,0.35),(0.2,0.12)]:
            setting=unreal.StaticMeshReductionSettings()
            setting.set_editor_property('percent_triangles',percent)
            setting.set_editor_property('screen_size',screen)
            settings.append(setting)
        reductions.set_editor_property('reduction_settings',settings)
        assert MESHES.set_lods(mesh,reductions)==3
        MESHES.remove_collisions(mesh)
        assert MESHES.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)>=0
        mesh.set_editor_property('light_map_resolution',64)
        assert LIB.save_loaded_asset(mesh,False)
        item=unreal.load_asset('/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_'+bag)
        assert item
        before={key:str(item.get_editor_property(key)) for key in ['item_id','rarity','bag_additional_slots','max_stack_size']}
        item.set_editor_property('icon',assets[bag]['icon'])
        item.set_editor_property('world_mesh',mesh)
        assert LIB.save_loaded_asset(item,False)
        assert before=={key:str(item.get_editor_property(key)) for key in before}
        bounds=mesh.get_bounding_box()
        box={'min':[bounds.min.x,bounds.min.y,bounds.min.z],'max':[bounds.max.x,bounds.max.y,bounds.max.z]}
        report={'bag':bag,'definition':item.get_path_name(),'icon':assets[bag]['icon'].get_path_name(),'mesh':mesh.get_path_name(),'lod_vertices':[MESHES.get_number_verts(mesh,i) for i in range(3)],'bounds_cm':box,'preserved':before}
        reports.append(report)
        unreal.log('BAG_IMPORTED '+json.dumps(report,ensure_ascii=False))
    (ROOT/'unreal_manifest.json').write_text(json.dumps(reports,indent=2,ensure_ascii=False),encoding='utf-8')
    unreal.log('BAG_IMPORTED all_assets=PASS bags=4')


if __name__=='__main__':
    import_bags()
