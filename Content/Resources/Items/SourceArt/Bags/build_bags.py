"""Author the four bag models in Blender; run --finalize to bake and export the saved scene."""
import bpy
import math
import json
import sys
from pathlib import Path
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parent
CM = 0.01
SPECS = [
    ("Bag01", 18, 6, 12, (0.28, 0.17, 0.085)),
    ("Bag02", 23, 11, 31, (0.25, 0.145, 0.075)),
    ("Bag03", 32, 17, 44, (0.185, 0.205, 0.105)),
    ("Bag04", 35, 19, 51, (0.115, 0.135, 0.065)),
]


def material(name, color, roughness=0.8, metallic=0.0, fabric=False):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.diffuse_color = (*color, 1)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if fabric:
        geo = nodes.new("ShaderNodeNewGeometry")
        macro = nodes.new("ShaderNodeTexNoise")
        macro.inputs["Scale"].default_value = 85
        macro.inputs["Detail"].default_value = 4
        links.new(geo.outputs["Position"], macro.inputs["Vector"])
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.15
        ramp.color_ramp.elements[0].color = (*(c * 0.65 for c in color), 1)
        ramp.color_ramp.elements[1].position = 0.85
        ramp.color_ramp.elements[1].color = (*(min(c * 1.2, 1) for c in color), 1)
        links.new(macro.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        micro = nodes.new("ShaderNodeTexNoise")
        micro.inputs["Scale"].default_value = 2600
        micro.inputs["Detail"].default_value = 2
        links.new(geo.outputs["Position"], micro.inputs["Vector"])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.24
        bump.inputs["Distance"].default_value = 0.00022
        links.new(micro.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


class Bag:
    def __init__(self, index, spec):
        self.name, self.w, self.d, self.h, color = spec
        self.collection = bpy.data.collections.new(self.name + "_Editable")
        bpy.context.scene.collection.children.link(self.collection)
        self.root = bpy.data.objects.new(self.name + "_Root", None)
        self.collection.objects.link(self.root)
        self.root.location.x = (index - 1.5) * 0.65
        self.parts = []
        self.cloth = material(self.name + "_Canvas", color, fabric=True)
        self.trim = material(self.name + "_Binding", tuple(c * 0.43 for c in color), fabric=True)
        self.strap = material(self.name + "_Webbing", (0.035, 0.033, 0.024), fabric=True)
        self.thread = material(self.name + "_Thread", tuple(c * 1.35 for c in color))
        self.plastic = material(self.name + "_Buckle", (0.022, 0.024, 0.02), 0.58)
        self.metal = material(self.name + "_Metal", (0.15, 0.135, 0.1), 0.42, 0.85)

    def attach(self, obj, mat):
        for col in list(obj.users_collection):
            col.objects.unlink(obj)
        self.collection.objects.link(obj)
        obj.parent = self.root
        obj.data.materials.append(mat)
        self.parts.append(obj)
        return obj

    def mesh(self, name, vertices, faces, mat):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata([tuple(v * CM for v in p) for p in vertices], [], faces)
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        self.collection.objects.link(obj)
        obj.parent = self.root
        mesh.materials.append(mat)
        for poly in mesh.polygons:
            poly.use_smooth = True
        self.parts.append(obj)
        return obj

    def box(self, name, center, size, mat, bevel=0.3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=tuple(c * CM for c in center))
        obj = bpy.context.object
        obj.name = name
        obj.dimensions = tuple(s * CM for s in size)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        self.attach(obj, mat)
        if bevel > 0:
            bevel_mod = obj.modifiers.new("Soft sewn edges", "BEVEL")
            bevel_mod.width = min(bevel, min(size) * 0.46) * CM
            bevel_mod.segments = 2 if bevel < 0.25 else 4
        normal = obj.modifiers.new("Weighted panel normals", "WEIGHTED_NORMAL")
        normal.keep_sharp = True
        for poly in obj.data.polygons:
            poly.use_smooth = True
        return obj

    def tube(self, name, points, radius, mat, closed=False):
        curve = bpy.data.curves.new(name, "CURVE")
        curve.dimensions = "3D"
        curve.resolution_u = 1
        curve.bevel_depth = radius * CM
        curve.bevel_resolution = 1
        curve.use_fill_caps = True
        spline = curve.splines.new("POLY")
        spline.points.add(len(points) - 1)
        for p, value in zip(spline.points, points):
            p.co = (*[c * CM for c in value], 1)
        spline.use_cyclic_u = closed
        obj = bpy.data.objects.new(name, curve)
        self.collection.objects.link(obj)
        obj.parent = self.root
        curve.materials.append(mat)
        self.parts.append(obj)
        return obj

    def panel(self, name, center, width, height, depth, radius, mat):
        x, y, z = center
        outline = []
        for cx, cz, start in [(width/2-radius, height/2-radius, 0), (-width/2+radius, height/2-radius, 90), (-width/2+radius, -height/2+radius, 180), (width/2-radius, -height/2+radius, 270)]:
            for j in range(7):
                a = math.radians(start + j*15)
                outline.append((x+cx+radius*math.cos(a), z+cz+radius*math.sin(a)))
        n = len(outline)
        vertices = [(px, y+dy, pz) for dy in [-depth/2, depth/2] for px,pz in outline]
        faces = [tuple(range(n)),tuple(reversed(range(n,2*n)))]
        faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        obj = self.mesh(name,vertices,faces,mat)
        obj.data.polygons[0].use_smooth = False
        obj.data.polygons[1].use_smooth = False
        return obj

    def front_point(self, x, z):
        t = max(-0.999,min(0.999,(z-self.h/2)/(self.h/2)))
        v = math.asin(math.copysign(abs(t)**(1/0.36),t))
        factor = math.cos(v)**0.36
        ring_t=(v+math.pi/2)/math.pi
        taper=1-0.13*max(0,(ring_t-0.5)*2)**2
        x=max(-0.98,min(0.98,x/(self.w/2*factor*taper)))*self.w/2*factor*taper
        cos_u=(abs(x)/(self.w/2*factor*taper))**(1/0.30)
        y=-self.d/2*factor*math.sqrt(max(0,1-cos_u*cos_u))**0.30
        y*=1+0.022*math.sin(x*0.6+ring_t*9)*math.sin(math.pi*ring_t)
        return (x,y-0.35,z)

    def border(self, name, center, width, height, radius=1.2, follow_body=False):
        x, y, z = center
        radius = min(radius, width / 3, height / 3)
        points = []
        for cx, cz, start in [(width/2-radius, height/2-radius, 0), (-width/2+radius, height/2-radius, 90), (-width/2+radius, -height/2+radius, 180), (width/2-radius, -height/2+radius, 270)]:
            for j in range(7):
                angle = math.radians(start + j * 15)
                points.append((x+cx+radius*math.cos(angle), y, z+cz+radius*math.sin(angle)))
        inner = [(x+(px-x)*0.967, y-0.035, z+(pz-z)*0.967) for px, _, pz in points]
        if follow_body:
            points=[self.front_point(px,pz) for px,_,pz in points]
            inner=[self.front_point(px,pz) for px,_,pz in inner]
        self.tube(name + "_Piping", points, 0.09, self.trim, True)
        self.tube(name + "_Stitch", inner, 0.024, self.thread, True)

    def ribbon(self, name, points, width, thickness, mat, closed=False):
        vertices, faces = [], []
        for i, p in enumerate(points):
            before = Vector(points[(i-1)%len(points)] if closed else points[max(0, i-1)])
            after = Vector(points[(i+1)%len(points)] if closed else points[min(len(points)-1, i+1)])
            tangent = (after-before).normalized()
            right = Vector((1, 0, 0))
            if abs(tangent.dot(right)) > 0.9:
                right = Vector((0, 1, 0))
            right = (right-tangent*right.dot(tangent)).normalized()
            normal = tangent.cross(right).normalized()
            for a, b in [(-1,-1), (1,-1), (1,1), (-1,1)]:
                vertices.append(Vector(p) + a*width/2*right + b*thickness/2*normal)
        for i in range(len(points) if closed else len(points)-1):
            next_i=(i+1)%len(points)
            for k in range(4):
                faces.append((i*4+k, i*4+(k+1)%4, next_i*4+(k+1)%4, next_i*4+k))
        if not closed:faces.extend([(3,2,1,0), tuple((len(points)-1)*4+k for k in range(4))])
        return self.mesh(name, vertices, faces, mat)

    def body(self):
        w, d, h = self.w, self.d, self.h
        vertices, faces = [(0, 0, 0)], []
        around, rings = 48, 24
        power = lambda x, e: math.copysign(abs(x)**e, x)
        for j in range(1, rings):
            v = -math.pi/2 + math.pi*j/rings
            t = j/rings
            for k in range(around):
                u = math.tau*k/around
                taper = 1 - (0.06 if self.name == "Bag01" else 0.13)*max(0, (t-0.5)*2)**2
                x = w/2 * power(math.cos(v), 0.36) * power(math.cos(u), 0.30) * taper
                y = d/2 * power(math.cos(v), 0.36) * power(math.sin(u), 0.30)
                y *= 1 + 0.022*math.sin(x*0.6 + t*9)*math.sin(math.pi*t)
                z = h/2 + h/2 * power(math.sin(v), 0.36)
                vertices.append((x, y, z))
        top = len(vertices)
        vertices.append((0, 0, h))
        for k in range(around):
            faces.append((0, 1+(k+1)%around, 1+k))
        for j in range(rings-2):
            a, b = 1+j*around, 1+(j+1)*around
            for k in range(around):
                faces.append((a+k, a+(k+1)%around, b+(k+1)%around, b+k))
        last = 1+(rings-2)*around
        for k in range(around):
            faces.append((last+k, last+(k+1)%around, top))
        self.mesh(self.name + "_SoftBody", vertices, faces, self.cloth)

    def shoulder_straps(self):
        w, d, h = self.w, self.d, self.h
        for side in [-1, 1]:
            points = []
            for i in range(19):
                t = i/18
                points.append((side*w*(0.24+0.04*math.sin(math.pi*t)), d/2+0.8+(14+h*0.1)*math.sin(math.pi*t), h*(0.94-0.79*t)+2.2*math.sin(math.tau*t)))
            self.ribbon(self.name + "_ShoulderPad", points[:13], w*0.15, 0.8, self.trim)
            self.ribbon(self.name + "_ShoulderWebbing", points[10:], w*0.075, 0.25, self.strap)
        handle = []
        for i in range(19):
            t = i/18
            handle.append((w*(-0.17+0.34*t), d*0.35, h-0.8+4.5*math.sin(math.pi*t)))
        self.ribbon(self.name + "_CarryHandle", handle, 1.6, 0.3, self.strap)

    def buckle(self, name, x, y, z, scale=1):
        a, b = 2.4*scale, 3.1*scale
        for dx in [-a/2, a/2]:
            self.box(name+"_Side", (x+dx,y,z), (0.45*scale,0.55*scale,b), self.plastic, 0.14)
        for dz in [-b/2, 0, b/2]:
            self.box(name+"_Cross", (x,y,z+dz), (a+0.4*scale,0.55*scale,0.4*scale), self.plastic, 0.13)
        self.box(name+"_Tongue", (x,y-0.2,z+0.5*scale), (0.65*scale,0.5*scale,1.1*scale), self.plastic,0.12)

    def zipper(self, name, x, y, z, width):
        self.tube(name+"_Tape", [(x-width/2,y,z),(x+width/2,y,z)],0.13,self.trim)
        for i in range(max(3,int(width/0.6))):
            px=x-width/2+(i+0.5)*0.6
            self.box(name+"_Tooth", (px,y-0.07,z), (0.23,0.16,0.25),self.metal,0)
        self.box(name+"_Slider", (x-width/2+0.5,y-0.15,z), (0.75,0.4,0.65),self.metal,0.15)
        pull=[(x-width/2+0.45,y-0.3,z),(x-width/2+0.4,y-0.65,z-1.0),(x-width/2+0.25,y-0.5,z-1.7)]
        self.ribbon(name+"_Pull",pull,0.65,0.15,self.strap)

    def pocket(self, name, z, width, height, depth):
        y=-self.d/2-depth/2+0.45
        self.box(name,(0,y,z),(width,depth,height),self.cloth,min(2.0,depth*0.38))
        front=y-depth/2-0.02
        self.border(name,(0,front,z),width-1,height-1,min(1.5,depth*0.3))
        self.zipper(name,0,front-0.13,z+height/2-1.3,width-2.4)
        return front

    def build(self):
        self.body()
        w,d,h=self.w,self.d,self.h
        if self.name == "Bag01":
            for side in [-1,1]:
                points=[(side*w*0.29,d/2+0.8+1.3*math.sin(math.tau*i/24),h*0.88+h*0.26*math.cos(math.tau*i/24)) for i in range(24)]
                self.ribbon("Pouch_BeltLoop",points,2.1,0.35,self.trim,True)
            self.panel("Pouch_Flap",(0,-d/2-0.3,h*0.68),w*0.98,h*0.65,0.6,2.0,self.cloth)
            self.border("Pouch_Flap",(0,-d/2-0.63,h*0.68),w*0.96,h*0.64,2.0)
            bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,location=(0,(-d/2-0.84)*CM,h*0.45*CM))
            button=bpy.context.object;button.name="Pouch_Snap";button.scale=(0.63*CM,0.20*CM,0.63*CM);self.attach(button,self.metal)
        else:
            self.shoulder_straps()
            if self.name == "Bag02":
                self.border("Daypack_MainSeam",(0,-d/2-0.02,h*0.51),w*0.89,h*0.90,3.3,True)
                self.pocket("Daypack_FrontPocket",h*0.25,w*0.83,h*0.37,3.8)
                self.zipper("Daypack_MainZip",0,-d/2-0.15,h*0.84,w*0.67)
            elif self.name == "Bag03":
                self.box("Hiking_ReinforcedBase",(0,0,2.3),(w*0.96,d*0.95,4.7),self.trim,1.7)
                self.box("Hiking_TopLid",(0,-0.3,h*0.91),(w*1.02,d*1.02,8.7),self.cloth,2.9)
                front=self.pocket("Hiking_FrontPocket",h*0.25,w*0.72,h*0.38,5)
                for side in [-1,1]:
                    x=side*w*0.27
                    self.box("Hiking_CompressionStrap",(x,-d/2-0.35,h*0.64),(1.8,0.32,h*0.48),self.strap,0.12)
                    self.buckle("Hiking_FrontBuckle",x,-d/2-0.9,h*0.65)
                    self.box("Hiking_SidePocket",(side*(w/2+1),0,h*0.29),(5.4,d*0.8,h*0.36),self.cloth,1.7)
                    self.box("Hiking_SideFlap",(side*(w/2+1),-0.3,h*0.48),(5.6,d*0.85,2.2),self.trim,0.8)
            else:
                self.box("Tactical_ReinforcedBase",(0,0,2.0),(w*0.96,d*0.96,4),self.trim,1.5)
                self.zipper("Tactical_MainZip",0,-d/2-0.1,h*0.90,w*0.67)
                for label,z,height,depth,width in [("Upper",h*0.69,h*0.31,5.3,w*0.80),("Lower",h*0.25,h*0.36,6.4,w*0.87)]:
                    front=self.pocket("Tactical_"+label,z,width,height,depth)
                    for row in range(3):
                        rz=z-height*0.26+row*height*0.24
                        self.box("Tactical_MOLLE_Band",(0,front-0.26,rz),(width*0.90,0.35,1.4),self.strap,0.12)
                        for col in range(6):
                            xx=width*(-0.43+col*0.172)
                            self.box("Tactical_Bartack",(xx,front-0.47,rz),(0.12,0.07,1.25),self.thread,0)
                for side in [-1,1]:
                    for z in [h*0.31,h*0.68]:
                        self.box("Tactical_SideCompression",(side*(w/2-0.15),0,z),(0.45,d*1.04,2.1),self.strap,0.18)
                        self.buckle("Tactical_SideBuckle",side*(w/2-0.1),-d*0.35,z,0.82)
        return self


def look_at(obj,target):
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


def setup_scene():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.context.preferences.filepaths.save_version=0
    scene=bpy.context.scene
    scene.unit_settings.system='METRIC'
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=32
    scene.render.resolution_x=768;scene.render.resolution_y=768;scene.render.resolution_percentage=100
    scene.render.film_transparent=True
    scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
    scene.world.color=(0.04,0.04,0.04)
    for name,loc,energy,size in [('Key',(-1.2,-1.8,2.1),65,1.3),('Fill',(1.5,-0.5,0.9),18,1.8),('Rim',(0.7,1.0,1.5),75,1.0)]:
        light=bpy.data.lights.new(name,'AREA');light.energy=energy;light.shape='DISK';light.size=size
        obj=bpy.data.objects.new(name,light);scene.collection.objects.link(obj);obj.location=loc;look_at(obj,(0,0,0.2))
    camera=bpy.data.objects.new('StudioCamera',bpy.data.cameras.new('StudioCamera'));scene.collection.objects.link(camera)
    camera.data.type='ORTHO';scene.camera=camera
    return scene


def render_preview(scene,bag):
    for col in bpy.data.collections:
        if col.name.endswith('_Editable'):col.hide_render=col!=bag.collection
    target=bag.root.location+Vector((0,0,bag.h*CM*0.53))
    scene.camera.location=target+Vector((0.9,-1.7,0.85));look_at(scene.camera,target)
    scene.camera.data.ortho_scale=max(bag.w*1.35,bag.h*1.28)*CM
    scene.render.filepath=str(ROOT/'Previews'/(bag.name+'_Mesh.png'))
    bpy.ops.render.render(write_still=True)


def export_mesh(collection,name):
    bpy.ops.object.select_all(action='DESELECT')
    copies=[]
    for obj in collection.objects:
        if obj.type not in {'MESH','CURVE'}:continue
        copy=obj.copy();copy.data=obj.data.copy();bpy.context.scene.collection.objects.link(copy)
        world=obj.matrix_world.copy();copy.parent=None;copy.matrix_world=world
        copy.hide_render=False;copy.hide_set(False);copy.select_set(True);copies.append(copy)
    bpy.context.view_layer.objects.active=copies[0]
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    obj=bpy.context.object;obj.name='SM_'+name;obj.location=(0,0,0)
    # Loose shoulder straps collapse against the back when this world prop rests on the ground.
    back=next(spec[2] for spec in SPECS if spec[0]==name)*CM/2
    for vertex in obj.data.vertices:
        if vertex.co.y>back:vertex.co.y=back+(vertex.co.y-back)*0.07
    obj.data.update()
    triangulate=obj.modifiers.new('Stable bake triangulation','TRIANGULATE')
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(70),island_margin=0.012,area_weight=0.8)
    bpy.ops.object.mode_set(mode='OBJECT')
    return obj


def bake_image(obj,name,kind,noncolor=False):
    image=bpy.data.images.new(name,width=1024,height=1024,alpha=True)
    image.colorspace_settings.name='Non-Color' if noncolor else 'sRGB'
    for mat in obj.data.materials:
        node=mat.node_tree.nodes.get('AtlasBakeTarget') or mat.node_tree.nodes.new('ShaderNodeTexImage')
        node.name='AtlasBakeTarget';node.image=image;mat.node_tree.nodes.active=node
    bpy.ops.object.bake(type=kind,margin=10,use_clear=True)
    return image


def finalize():
    import numpy as np
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Bags.blend'))
    bpy.context.preferences.filepaths.save_version=0
    for obj in list(bpy.data.objects):
        if obj.name.startswith('SM_Bag'):bpy.data.objects.remove(obj,do_unlink=True)
    for mat in list(bpy.data.materials):
        if mat.name.endswith('_Atlas'):bpy.data.materials.remove(mat,do_unlink=True)
    for image in list(bpy.data.images):
        if image.name.startswith('T_Bag') or image.name.endswith(('_Roughness','_AO','_Metallic')):bpy.data.images.remove(image,do_unlink=True)
    scene=bpy.context.scene;scene.cycles.samples=16
    scene.render.bake.use_pass_direct=False;scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
    for col in bpy.data.collections:
        if col.name.endswith('_Editable'):col.hide_render=True
    stats=[]
    for name,*_ in SPECS:
        obj=export_mesh(bpy.data.collections[name+'_Editable'],name)
        bpy.context.view_layer.objects.active=obj
        base=bake_image(obj,'T_'+name+'_BaseColor','DIFFUSE')
        normal=bake_image(obj,'T_'+name+'_Normal','NORMAL',True)
        rough=bake_image(obj,name+'_Roughness','ROUGHNESS',True)
        ao=bake_image(obj,name+'_AO','AO',True)
        restore=[]
        for mat in obj.data.materials:
            nodes,links=mat.node_tree.nodes,mat.node_tree.links
            output=nodes.get('Material Output');old=output.inputs['Surface'].links[0].from_socket
            emission=nodes.new('ShaderNodeEmission');metal=nodes.get('Principled BSDF').inputs['Metallic'].default_value
            emission.inputs['Color'].default_value=(metal,metal,metal,1)
            links.new(emission.outputs[0],output.inputs['Surface']);restore.append((mat,output,old,emission))
        metal=bake_image(obj,name+'_Metallic','EMIT',True)
        for mat,output,old,emission in restore:
            mat.node_tree.links.new(old,output.inputs['Surface']);mat.node_tree.nodes.remove(emission)
        packed=np.ones(1024*1024*4,dtype=np.float32)
        for channel,img in [(0,ao),(1,rough),(2,metal)]:
            pixels=np.empty_like(packed);img.pixels.foreach_get(pixels);packed[channel::4]=pixels[0::4]
        orm=bpy.data.images.new('T_'+name+'_ORM',width=1024,height=1024,alpha=True)
        orm.colorspace_settings.name='Non-Color';orm.pixels.foreach_set(packed)
        for image in [base,normal,orm]:
            image.filepath_raw=str(ROOT/'Textures'/(image.name+'.png'));image.file_format='PNG';image.save()
        original_materials = len(obj.data.materials)
        atlas=material(name+'_Atlas',(1,1,1))
        nodes,links=atlas.node_tree.nodes,atlas.node_tree.links
        bsdf=nodes.get('Principled BSDF')
        textures=[]
        for image in [base,normal,orm]:
            node=nodes.new('ShaderNodeTexImage');node.image=image;textures.append(node)
        links.new(textures[0].outputs['Color'],bsdf.inputs['Base Color'])
        normal_map=nodes.new('ShaderNodeNormalMap');links.new(textures[1].outputs['Color'],normal_map.inputs['Color']);links.new(normal_map.outputs['Normal'],bsdf.inputs['Normal'])
        split=nodes.new('ShaderNodeSeparateColor');links.new(textures[2].outputs['Color'],split.inputs['Color'])
        links.new(split.outputs['Green'],bsdf.inputs['Roughness']);links.new(split.outputs['Blue'],bsdf.inputs['Metallic'])
        obj.data.materials.clear();obj.data.materials.append(atlas)
        for poly in obj.data.polygons:poly.material_index=0
        # Apply an orientation-preserving rotation: bag front points up when dropped.
        obj.data.transform(Matrix(((0,0,1,0),(-1,0,0,0),(0,-1,0,0),(0,0,0,1))))
        minimum=min(v.co.z for v in obj.data.vertices)
        cx=(min(v.co.x for v in obj.data.vertices)+max(v.co.x for v in obj.data.vertices))/2
        cy=(min(v.co.y for v in obj.data.vertices)+max(v.co.y for v in obj.data.vertices))/2
        for v in obj.data.vertices:v.co-=Vector((cx,cy,minimum))
        obj.data.update()
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        bpy.ops.export_scene.fbx(filepath=str(ROOT/'Meshes'/('SM_'+name+'.fbx')),use_selection=True,object_types={'MESH'},add_leaf_bones=False,axis_forward='X',axis_up='Z',use_mesh_modifiers=True,use_tspace=True,path_mode='STRIP',bake_anim=False)
        stats.append({'name':name,'vertices':len(obj.data.vertices),'triangles':len(obj.data.polygons),'material_slots':len(obj.data.materials),'materials_before_atlas':original_materials})
        bpy.context.view_layer.update()
        size=[max(v.co[i] for v in obj.data.vertices)-min(v.co[i] for v in obj.data.vertices) for i in range(3)]
        target=Vector((0,0,size[2]/2))
        scene.camera.location=target+Vector((0.8,-1.25,2.1));look_at(scene.camera,target)
        scene.camera.data.ortho_scale=max(size)*1.45
        scene.render.filepath=str(ROOT/'Previews'/(name+'_WorldMesh.png'))
        scene.cycles.samples=32;bpy.ops.render.render(write_still=True);scene.cycles.samples=16
        obj.hide_render=True;obj.hide_set(True)
    (ROOT/'mesh_manifest.json').write_text(json.dumps(stats,indent=2),encoding='utf-8')
    for col in bpy.data.collections:
        if col.name.endswith('_Editable'):col.hide_render=False
    bpy.ops.file.pack_all()
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Bags.blend'))
    print('BAG_EXPORT_COMPLETE '+json.dumps(stats))


if __name__=='__main__':
    ROOT.mkdir(parents=True,exist_ok=True)
    for folder in ['Meshes','Textures','Previews']:(ROOT/folder).mkdir(exist_ok=True)
    if '--finalize' in sys.argv:
        finalize()
    else:
        scene=setup_scene()
        bags=[Bag(i,spec).build() for i,spec in enumerate(SPECS)]
        bpy.context.view_layer.update()
        for bag in bags:render_preview(scene,bag)
        for bag in bags:bag.collection.hide_render=False
        bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Bags.blend'))
        print('BAG_MODELS_COMPLETE')
