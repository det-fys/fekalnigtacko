from __future__ import annotations
from dataclasses import dataclass
import bpy
import os
import re
import math

CHUNK_SIZE = 250.0

@dataclass
class Vec3:
    x: float
    y: float
    z: float

    def __add__(self, other):
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __mul__(self, scalar: float):
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)

    def dot(self, other):
        return self.x*other.x + self.y*other.y + self.z*other.z

    def cross(self, other):
        return Vec3(
            self.y*other.z - self.z*other.y,
            self.z*other.x - self.x*other.z,
            self.x*other.y - self.y*other.x
        )
    
    @staticmethod
    def min(a: Vec3, b: Vec3):
        return Vec3(
            a.x if a.x < b.x else b.x,
            a.y if a.y < b.y else b.y,
            a.z if a.z < b.z else b.z
        )

    @staticmethod
    def max(a: Vec3, b: Vec3):
        return Vec3(
            a.x if a.x > b.x else b.x,
            a.y if a.y > b.y else b.y,
            a.z if a.z > b.z else b.z
        )


    def norm(self):
        return math.sqrt(self.dot(self))

class Vertex:
    position: tuple[float, float, float]
    normal: tuple[float, float, float]
    uv: tuple[float, float]
    color: tuple[float, float, float] | None

    def __init__(self, position, normal, uv, color=None):
        self.position = tuple(round(x, 6) for x in position)
        self.normal = tuple(round(x, 6) for x in normal)
        self.uv = tuple(round(x, 6) for x in uv)
        self.color = tuple(round(c, 3) for c in color) if color else None

    def __hash__(self):
        return hash((self.position, self.normal, self.uv, self.color))
    
    def __eq__(self, other):
        return (self.position, self.normal, self.uv, self.color) == (other.position, other.normal, other.uv, other.color)

class Surface:
    tris: list[tuple[int, int, int]]
    texture: str
    twosided: bool
    ocolor: bool
    blend: str|None

    def __init__(self, name: str):
        self.tris = []
        self.texture = name
        self.twosided = False
        self.ocolor = False
        self.blend = None

class Model:
    vertices: list[Vertex]
    vertex_map: dict[Vertex, int]
    materials: dict[str, Surface]
    make_col_trimesh: bool
    make_convex_hull: bool

    def __init__(self):
        self.vertices = []
        self.vertex_map = {}
        self.materials = {}
        self.make_col_trimesh = False
        self.make_convex_hull = False

    def add_vertex(self, vertex: Vertex) -> int:
        if vertex in self.vertex_map:
            return self.vertex_map[vertex]
        index = len(self.vertices)
        self.vertices.append(vertex)
        self.vertex_map[vertex] = index
        return index
    
    def add_triangle(self, material_name: str, v1: int, v2: int, v3: int):
        # if material_name not in self.materials:
        #     self.materials[material_name] = Surface(material_name)
        self.materials[material_name].tris.append((v1, v2, v3))

class Transform:
    position: tuple[float, float, float]
    rotation: tuple[float, float, float, float] # quat xyzw
    scale: float

    def __init__(self, position, rotation, scale):
        self.position = tuple(round(x, 6) for x in position)
        self.rotation = tuple(round(x, 6) for x in rotation)
        self.scale = round(scale, 6)

class GraphNode:
    pos: tuple[float, float, float]
    
    def __init__(self, pos):
        self.pos = tuple(round(x, 6) for x in pos)

class GraphEdge:
    nodes: tuple[int, int]

    def __init__(self, node1, node2):
        self.nodes = (node1, node2)

class Graph:
    name: str
    nodes: list[GraphNode]
    edges: list[GraphEdge]
    
    def __init__(self, name: str):
        self.name = name
        self.nodes = []
        self.edges = []

    def add_node(self, node: GraphNode) -> int:
        index = len(self.nodes)
        self.nodes.append(node)
        return index
    
    def add_edge(self, node1_index: int, node2_index: int):
        edge = GraphEdge(node1_index, node2_index)
        self.edges.append(edge)

class Chunk:
    coord: tuple[int, int] # x,y
    shifted_coord: tuple[int, int]
    aabb_min: Vec3
    aabb_max: Vec3
    static_objects: list[tuple[str, Transform]]
    surface_ranges: list[tuple[str, int, int]] # name, first, count

    def __init__(self, coord: tuple[int, int]):
        self.coord = coord
        self.shifted_coord = (0, 0)
        self.static_objects = []
        self.surface_ranges = []

        self.aabb_min = Vec3(math.inf, math.inf, math.inf)
        self.aabb_max = Vec3(-math.inf, -math.inf, -math.inf)

    def extend_aabb(self, pos: Vec3):
        self.aabb_min = Vec3.min(self.aabb_min, pos)
        self.aabb_max = Vec3.max(self.aabb_max, pos)

class Map:
    basemodel: Model
    basemodel_name: str
    static_objects: list[tuple[str, Transform]]
    graphs: list[Graph]
    chunks: dict[tuple[int, int], Chunk]
    max_chunk: tuple[int, int]

    def __init__(self):
        self.basemodel = Model()
        self.basemodel_name = ""
        self.static_objects = []
        self.graphs = []
        self.chunks = None
        self.max_chunk = (0, 0)

    def create_chunks(self):
        self.chunks = {}

        def get_chunk(coord: tuple[int,int]) -> Chunk:
            if not coord in self.chunks:
                chunk = Chunk(coord)
                self.chunks[coord] = chunk
                return chunk
            return self.chunks[coord]
        
        def pos_to_chunk(pos: Vec3) -> tuple[int,int]:
            return int(math.floor(pos.x / CHUNK_SIZE)), int(math.floor(pos.y / CHUNK_SIZE))
        
        def tri_to_chunk(tri: tuple[int, int, int]) -> tuple[int, int]:
            p0, p1, p2 = [Vec3(*self.basemodel.vertices[i].position) for i in tri]
            return pos_to_chunk((p0 + p1 + p2) * (1/3))

        # surfaces
        for name, surface in self.basemodel.materials.items():
            tris = [(tri, tri_to_chunk(tri)) for tri in surface.tris]
            tris.sort(key=lambda x: x[1]) #sort by chunk coord

            cur_chunk: Chunk|None = None
            start = 0
            i = 0

            def finish_cur_chunk():
                if cur_chunk is None or i - start < 1:
                    return

                count = i - start
                cur_chunk.surface_ranges.append((name, start, count))

                # extend aabb wtih tris
                for j in range(start, i):
                    p0, p1, p2 = [Vec3(*self.basemodel.vertices[k].position) for k in tris[j][0]]
                    cur_chunk.extend_aabb(p0)
                    cur_chunk.extend_aabb(p1)
                    cur_chunk.extend_aabb(p2)


            surface.tris.clear()

            for tri_coord in tris:
                tri, coord = tri_coord
                surface.tris.append(tri)

                if cur_chunk is None or coord != cur_chunk.coord:
                    finish_cur_chunk()
                    cur_chunk = get_chunk(coord)
                    start = i

                i += 1

            finish_cur_chunk()

        # objects
        for obj in self.static_objects:
            name, trans = obj
            pos = Vec3(*trans.position)

            chunk = get_chunk(pos_to_chunk(pos))
            chunk.extend_aabb(pos)
            chunk.static_objects.append(obj)

        # min/max
        min_chunk = (100000, 100000)
        max_chunk = (-100000, -100000)

        for coord in self.chunks:
            min_chunk = (
                min_chunk[0] if min_chunk[0] < coord[0] else coord[0],
                min_chunk[1] if min_chunk[1] < coord[1] else coord[1]
            )

            max_chunk = (
                max_chunk[0] if max_chunk[0] > coord[0] else coord[0],
                max_chunk[1] if max_chunk[1] > coord[1] else coord[1]
            )

        for coord, chunk in self.chunks.items():
            chunk.shifted_coord = (
                coord[0] - min_chunk[0],
                coord[1] - min_chunk[1],
            )

        self.max_chunk = (
            max_chunk[0] - min_chunk[0],
            max_chunk[1] - min_chunk[1],
        )

class Wheel:
    type: str
    model_name: str
    position: tuple[float, float, float]
    radius: float

class Vehicle:
    basemodel_name: str
    wheels: list[Wheel] # type, model, transform

    def __init__(self):
        self.wheels = []

class Bone:
    name: str
    parent: str|None
    trans: Transform

    def __init__(self, name):
        self.name = name

class AnimChannel:
    name: str
    frames: list[tuple[int, Transform]]

    def __init__(self, name):
        self.name = name
        self.frames = []

class Animation:
    name: str
    fps: float
    frames: int
    channels: list[AnimChannel]

    def __init__(self, name: str, fps: float, frames: int):
        self.name = name
        self.fps = fps
        self.frames = frames
        self.channels = []

class Skeleton:
    name: str
    bones: list[Bone]
    bone_set: set[str]
    anims: list[Animation]

    def __init__(self, name, armature):
        self.name = name
        self.bones = []
        self.bone_set = set()
        self.armature = armature
        self.anims = []

class Exporter:
    skeletons: dict[str, Skeleton]

    def __init__(self):
        self.blend_dir = os.path.dirname(bpy.data.filepath)
        self.out_path = os.path.join(self.blend_dir, "export")
        self.skeletons = {}

    @staticmethod
    def add_mesh_to_model(obj, model: Model):
        if obj.type != "MESH":
            print("warning: tried to export object that is not a mesh")
            return
        
        print(f"  processing mesh: {obj.name}")

        mesh = obj.data
        mesh.calc_loop_triangles()

        # Prepare UV layers
        uv_layer = mesh.uv_layers.active.data

        for tri in mesh.loop_triangles:
            face_indices = []

            material_index = tri.material_index

            # Get the material from the object's material slots
            if material_index < len(obj.material_slots):
                material = obj.material_slots[material_index].material
                material_name = material.name if material else "Unknown"
            else:
                material_name = "Unknown"

            mat_type, mat_name, mat_params = Exporter.extract_name(material_name)

            if mat_type != "MAT":
                continue # skip non material

            for loop_index in tri.loops:
                loop = mesh.loops[loop_index]
                vertex = mesh.vertices[loop.vertex_index]

                pos = tuple(c for c in vertex.co)
                normal = tuple(n for n in loop.normal)
                uv = tuple(c for c in uv_layer[loop_index].uv)

                # get color from named attribute
                color = None
                color_attr = mesh.color_attributes.get("vertex_color")
                if color_attr:
                    color_data = color_attr.data[loop_index]
                    color = tuple(c for c in color_data.color[:3]) # ignore alpha

                vert = Vertex(position=pos, normal=normal, uv=uv, color=color)
                vert_index = model.add_vertex(vert)

                face_indices.append(vert_index)

            if mat_name not in model.materials:
                surface = Surface(mat_name)
                surface.twosided = "2S" in mat_params
                surface.ocolor = "OCOLOR" in mat_params
                blend = mat_params.get("BLEND")
                if isinstance(blend, str):
                    surface.blend = blend
                model.materials[mat_name] = surface

            model.add_triangle(mat_name, *face_indices)

    @staticmethod
    def add_mesh_to_graph(obj, graph: Graph):
        mesh = obj.data

        # Ensure we have access to the 'G_flip' attribute
        flip_layer = None
        if "G_flip" in mesh.attributes:
            flip_layer = mesh.attributes["G_flip"]

        # Add nodes
        for v in mesh.vertices:
            pos = tuple(c for c in v.co)
            graph.add_node(GraphNode(pos))

        # Add edges
        for e in mesh.edges:
            v1, v2 = e.vertices[0], e.vertices[1]

            if flip_layer and flip_layer.data[e.index].value:
                graph.add_edge(v2, v1)
            else:
                graph.add_edge(v1, v2)

    def rad2deg(self, rad: float) -> float:
        return rad * (180.0 / 3.141592653589793)

    def transform_str(self, transform: Transform):
        # rx, ry, rz = transform.rotation
        # dx, dy, dz = self.rad2deg(rx), self.rad2deg(ry), self.rad2deg(rz)
        t, r, s = transform.position, transform.rotation, transform.scale
        return f"{t[0]:.6f} {t[1]:.6f} {t[2]:.6f} {r[0]:.6f} {r[1]:.6f} {r[2]:.6f} {r[3]:.6f} {s:.6f}"

    def export_mdl(self, model: Model, filepath: str):
        with open(filepath, "w") as f:
            if model.make_col_trimesh:
                f.write("makecoltrimesh\n")

            if model.make_convex_hull:
                f.write("makeconvexhull\n")

            for v in model.vertices:
                color_str = f" {v.color[0]} {v.color[1]} {v.color[2]}" if v.color else ""
                f.write(f"v {v.position[0]} {v.position[1]} {v.position[2]} {v.normal[0]} {v.normal[1]} {v.normal[2]} {v.uv[0]} {v.uv[1]}{color_str}\n")
            
            for mat_name, surface in model.materials.items():
                f.write(f"surface {mat_name} +texture {surface.texture}")
                if surface.twosided:
                    f.write(" +2sided")
                if surface.ocolor:
                    f.write(" +ocolor")
                if surface.blend is not None:
                    f.write(f" +blend {surface.blend}")
                f.write("\n")
                for tri in surface.tris:
                    f.write(f"f {tri[0]} {tri[1]} {tri[2]}\n")
    
    def export_map(self, map: Map, filepath: str):
        with open(filepath, "w") as f:
            f.write(f"basemodel {map.basemodel_name}\n")
            
            # graphs
            for graph in map.graphs:
                f.write(f"graph {graph.name}\n")
                for node in graph.nodes:
                    f.write(f"n {node.pos[0]} {node.pos[1]} {node.pos[2]}\n")
                for edge in graph.edges:
                    f.write(f"e {edge.nodes[0]} {edge.nodes[1]}\n")

            # # static
            # for obj_name, transform in map.static_objects:
            #     f.write(f"static {obj_name} {Exporter.transform_str(transform)}\n")

            f.write(f"chunks {map.max_chunk[0]} {map.max_chunk[1]}\n")

            chunks_sorted = [c[1] for c in map.chunks.items()]
            chunks_sorted.sort(key=lambda c: c.shifted_coord)

            for chunk in chunks_sorted:
                f.write(f"chunk {chunk.shifted_coord[0]} {chunk.shifted_coord[1]} {chunk.aabb_min.x} {chunk.aabb_min.y} {chunk.aabb_min.z} {chunk.aabb_max.x} {chunk.aabb_max.y} {chunk.aabb_max.z}\n")

                for name, first, count in chunk.surface_ranges:
                    f.write(f"surface {name} {first} {count}\n")

                for obj_name, transform in chunk.static_objects:
                    f.write(f"static {obj_name} {self.transform_str(transform)}\n")

    def export_veh(self, veh: Vehicle, filepath: str):
        with open(filepath, "w") as f:
            f.write(f"basemodel {veh.basemodel_name}\n")
            
            for wheel in veh.wheels:
                f.write(f"wheel {wheel.type} {wheel.model_name} {wheel.position[0]} {wheel.position[1]} {wheel.position[2]} {wheel.radius}\n")
    
    def export_sk(self, sk: Skeleton, filepath: str):
        with open(filepath, "w") as f:
            for bone in sk.bones:
                parent_str = bone.parent if bone.parent is not None else "NONE"
                f.write(f"b {bone.name} {parent_str} {self.transform_str(bone.trans)}\n")

            for anim in sk.anims:
                f.write(f"anim {anim.name} {sk.name}_{anim.name}\n")
                
    def export_anim(self, anim: Animation, filepath: str):
        with open(filepath, 'w') as f:
            f.write(f"frames {anim.frames}\n")
            f.write(f"fps {anim.fps}\n")

            for chan in anim.channels:
                f.write(f"ch {chan.name}\n")

                for idx, trans in chan.frames:
                    f.write(f"f {idx} {self.transform_str(trans)}\n")

    @staticmethod
    def extract_name(fullname: str) -> tuple[str|None, str, dict[str, str|bool]]:
        match = re.match(rf"^(\w+)\/([\w]+)\/?([^\.]*)", fullname)
        
        if not match:
            return None, fullname, {}
        
        type, name, all_params_str = match.group(1), match.group(2), match.group(3)

        # extract params
        params_str = all_params_str.split("/")
        params: dict[str, str|bool] = {}
        for str_param in params_str:
            if str_param.find("=") == -1:
                params[str_param] = True
                continue
            
            k, v = str_param.split("=")
            params[k] = v
        
        return type, name, params
    
    def get_obj_transform(self, obj) -> Transform:
        mat = obj.matrix_world
        # position = obj.location
        # rotation = (obj.rotation.x, obj.rotation.y, obj.rotation.z, obj.rotation.w)
        # scale = obj.scale[0] # assume uniform scale
        # return Transform(position, rotation, scale)
        return Transform(*self.matrix_decompose(mat))

    def process_MDL(self, col, name, params):
        print(f"exporting MDL: {name}")
        model = Model()

        if "C" in params:
            Cs = params["C"].split(",")
            for C in Cs:
                if C == "tri":
                    model.make_col_trimesh = True
                elif C == "convex":
                    model.make_convex_hull = True

        for obj in col.objects:
            type, _, _ = self.extract_name(obj.name)

            if type == "M":
                self.add_mesh_to_model(obj, model)

        mdl_filepath = os.path.join(self.out_path, f"{name}.mdl")
        self.export_mdl(model, mdl_filepath)

    def process_MAP(self, col, name, params):
        print(f"exporting MAP: {name}")
        map = Map()
        map.basemodel_name = name
        map.basemodel.make_col_trimesh = True

        def proc_col(col):
            for obj in col.objects:
                type, obj_name, _ = self.extract_name(obj.name)

                if type == "M":
                    self.add_mesh_to_model(obj, map.basemodel)
                elif type == "OBJ":
                    transform = self.get_obj_transform(obj)
                    map.static_objects.append((obj_name, transform))
                elif type == "GRAPH":
                    graph = Graph(obj_name)
                    self.add_mesh_to_graph(obj, graph)
                    map.graphs.append(graph)

            for child_col in col.children:
                proc_col(child_col)

        proc_col(col)

        map.create_chunks()

        mdl_filepath = os.path.join(self.out_path, f"{name}.mdl")
        self.export_mdl(map.basemodel, mdl_filepath)

        map_filepath = os.path.join(self.out_path, f"{name}.map")
        self.export_map(map, map_filepath)

    def process_VEH(self, col, name, params):
        print(f"exporting VEH: {name}")
        veh = Vehicle()
        veh.basemodel_name = name

        for obj in col.objects:
            type, obj_name, params = self.extract_name(obj.name)

            if type == "WHEEL":
                wheel_type = params["W"] if "W" in params else ""
                radius = float(params["R"].replace(",", ".")) if "R" in params else 0.5
                transform = self.get_obj_transform(obj)

                wheel = Wheel()
                wheel.type = wheel_type
                wheel.model_name = obj_name
                wheel.position = transform.position
                wheel.radius = radius
                veh.wheels.append(wheel)

        veh.wheels.sort(key=lambda w: w.type)

        veh_filepath = os.path.join(self.out_path, f"{name}.veh")
        self.export_veh(veh, veh_filepath)

    def get_armature_keep_bones(self, armature):
        keep_bones = set()
        
        for bone in armature.data.bones:
            #bone_name = bone.name

            if bone.use_deform: # or is_tag:
                keep_bones.add(bone)

                while bone.parent:
                    bone = bone.parent
                    keep_bones.add(bone)

        return keep_bones

    def matrix_decompose(self, matrix):
        t, r, s = matrix.decompose()
        return (t.x, t.y, t.z), (r.x, r.y, r.z, r.w), s.x

    def process_SK(self, obj, name, params):
        if obj.type != "ARMATURE":
            print("SK object not armature!")
            return
        
        sk = Skeleton(name, obj)

        keep_bones = self.get_armature_keep_bones(obj)

        keep_bones_str = ",".join([bone.name for bone in keep_bones])
        print(f"Keep bones: {keep_bones_str}")
        print(f"Num bones: {len(keep_bones)}")
        
        for bone in obj.data.bones:
            if not bone in keep_bones:
                continue

            xbone = Bone(bone.name)

            parent = bone.parent
            xbone.parent = parent.name if parent else None

            bind_matrix = bone.matrix_local
            xbone.trans = Transform(*self.matrix_decompose(bind_matrix))

            sk.bones.append(xbone)
            sk.bone_set.add(xbone.name)

        self.skeletons[name] = sk

    def process_A(self, action, name, params):
        if not "_" in name:
            print(f"{name}: required format skeleton_animname")
            return

        sk_name, anim_name = name.split("_", 1)

        if not sk_name in self.skeletons:
            print(f"{anim_name}: unknown anim skeleton {sk_name}")
            return

        sk = self.skeletons[sk_name]

        original_action = sk.armature.animation_data.action
        original_frame = bpy.context.scene.frame_current

        sk.armature.animation_data.action = action

        bone_frames = {bonename: [] for bonename in sk.bone_set}

        _, end = map(int, action.frame_range)
        fps = bpy.context.scene.render.fps

        def vectors_similar(v1, v2, threshold):
            return (v1 - v2).length < threshold

        def quats_similar(q1, q2, threshold=1e-4):
            return q1.rotation_difference(q2).angle < threshold

        def frames_similar(f1, f2, threshold=0.00001):
            _, t1, r1, s1, _ = f1
            _, t2, r2, s2, _ = f2

            if not vectors_similar(t1, t2, threshold):
                return False

            if not quats_similar(r1, r2):
                return False

            if not vectors_similar(s1, s2, threshold):
                return False

            return True

        for frame in range(0, end):
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()

            for bonename in sk.bone_set:
                pose_bone = sk.armature.pose.bones.get(bonename)

                if not pose_bone:
                    continue

                matrix = (pose_bone.parent.matrix.inverted() @ pose_bone.matrix) if pose_bone.parent else pose_bone.matrix.copy()

                translation, rotation, scale = matrix.decompose()
                current_frame = (frame, translation, rotation, scale, matrix)

                frame_list = bone_frames[bonename]

                if len(frame_list) > 0:
                    last_frame = frame_list[-1]

                    if frames_similar(last_frame, current_frame):
                        continue

                frame_list.append(current_frame)

        anim = Animation(anim_name, fps, end)

        for bone_name, frames in bone_frames.items():
            chan = AnimChannel(bone_name)

            for frame in frames:
                idx, _, _, _, matrix = frame
                chan.frames.append((idx, Transform(*self.matrix_decompose(matrix))))

            anim.channels.append(chan)

        anim.channels.sort(key=lambda ch: ch.name)

        bpy.context.scene.frame_set(original_frame)
        sk.armature.animation_data.action = original_action

        sk.anims.append(anim)

    def run(self):
        print("=== EXPORT STARTED ===")
        os.makedirs(self.out_path, exist_ok=True)

        # collections
        for col in bpy.context.scene.collection.children:
            type, name, params = self.extract_name(col.name)

            if type == "MDL":
                self.process_MDL(col, name, params)
            elif type == "MAP":
                self.process_MAP(col, name, params)
            elif type == "VEH":
                self.process_VEH(col, name, params)

        # skeletons
        for obj in bpy.context.scene.collection.objects:
            type, name, params = self.extract_name(obj.name)

            if type == "SK":
                self.process_SK(obj, name, params)

        # animations
        for action in bpy.data.actions:
            type, name, params = self.extract_name(action.name)

            if type == "A":
                self.process_A(action, name, params)

        # export skeletons & anims
        for name, sk in self.skeletons.items():
            sk.anims.sort(key=lambda a: a.name)
            for anim in sk.anims:
                self.export_anim(anim, os.path.join(self.out_path, f"{name}_{anim.name}.anim"))
            self.export_sk(sk, os.path.join(self.out_path, f"{name}.sk"))


Exporter().run()
