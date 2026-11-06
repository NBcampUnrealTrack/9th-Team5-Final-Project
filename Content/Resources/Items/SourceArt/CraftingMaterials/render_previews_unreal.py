"""Render imported meshes in a ticking editor: -RenderOffscreen -ExecutePythonScript=<this file>."""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parent
OUT = ROOT/'Previews'; OUT.mkdir(exist_ok=True)
world = unreal.EditorLevelLibrary.get_editor_world()
spawned = []


def spawn(cls, position, target=None):
    rotation = unreal.MathLibrary.find_look_at_rotation(position, target) if target else unreal.Rotator()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, position, rotation)
    assert actor
    spawned.append(actor)
    return actor


try:
    origin = unreal.Vector(20000, 20000, 20000)
    prop = spawn(unreal.StaticMeshActor, origin)
    mesh_component = prop.get_component_by_class(unreal.StaticMeshComponent)
    mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky = spawn(unreal.SkyLight, origin)
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cube = unreal.load_asset('/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1')
    assert isinstance(cube, unreal.TextureCube)
    sky_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky_component.set_editor_property('cubemap', cube)
    sky_component.set_editor_property('intensity', .7)
    sky_component.recapture_sky()
    for offset, intensity, color in [((-55, -65, 100), 40., (1., .95, .86)), ((75, -20, 60), 25., (.8, .9, 1.)), ((20, 80, 70), 45., (1., 1., 1.))]:
        position = origin+unreal.Vector(*offset)
        actor = spawn(unreal.RectLight, position, origin+unreal.Vector(0, 0, 8))
        component = actor.get_component_by_class(unreal.RectLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('intensity', intensity)
        component.set_editor_property('source_width', 80.)
        component.set_editor_property('source_height', 80.)
        component.set_light_color(unreal.LinearColor(*color, 1))
    camera = spawn(unreal.SceneCapture2D, origin+unreal.Vector(65, -90, 75), origin+unreal.Vector(0, 0, 8))
    capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    capture.set_editor_property('capture_every_frame', False)
    capture.set_editor_property('capture_on_movement', False)
    capture.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    settings = capture.get_editor_property('post_process_settings')
    for key, value in dict(override_auto_exposure_method=True, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
                           override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
                           override_auto_exposure_bias=True, auto_exposure_bias=-1.).items(): settings.set_editor_property(key, value)
    capture.set_editor_property('post_process_settings', settings)
    target = unreal.RenderingLibrary.create_render_target2d(world, 1024, 1024, unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(.035, .04, .045, 1))
    capture.set_editor_property('texture_target', target)
    specs = json.loads((ROOT/'mesh_manifest.json').read_text(encoding='utf-8'))
    state = {'frame': 0, 'index': 0}

    def select_mesh():
        spec = specs[state['index']]
        name = spec['name']
        mesh = unreal.load_asset('/Game/Resources/Items/Mesh/Materials/SM_Item_'+name)
        mesh_component.set_static_mesh(mesh)
        unreal.AutomationLibrary.finish_loading_before_screenshot()
        bounds = mesh.get_bounding_box()
        center = origin+unreal.Vector(0, 0, (bounds.min.z+bounds.max.z)/2)
        distance = max(bounds.max.x-bounds.min.x, bounds.max.y-bounds.min.y, bounds.max.z-bounds.min.z)*1.5
        capture.set_editor_property('ortho_width', distance)
        camera.set_actor_location(center+unreal.Vector(65, -90, 75), False, False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), center), False)
        capture.set_editor_property('capture_every_frame', True)
        unreal.log('CRAFT_ART_FRAME '+name+' width='+str(distance)+' center='+str(center))

    def finish():
        unreal.unregister_slate_post_tick_callback(state['handle'])
        for actor in reversed(spawned): unreal.EditorLevelLibrary.destroy_actor(actor)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def tick(delta):
        try:
            state['frame'] += 1
            if state['frame'] == 1: select_mesh()
            if state['frame'] == 60: unreal.AutomationLibrary.finish_loading_before_screenshot()
            if state['frame'] == 90:
                unreal.EditorLevelLibrary.editor_invalidate_viewports()
                capture.capture_scene()
            if state['frame'] < 120: return
            name = specs[state['index']]['name']
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name+'_WorldMesh.png')
            unreal.log('CRAFT_ART_PREVIEW '+name)
            state['index'] += 1
            if state['index'] == len(specs):
                finish()
            else:
                state['frame'] = 0
        except Exception:
            import traceback
            unreal.log_error(traceback.format_exc())
            finish()

    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    state['handle'] = unreal.register_slate_post_tick_callback(tick)
except Exception:
    for actor in reversed(spawned): unreal.EditorLevelLibrary.destroy_actor(actor)
    raise
