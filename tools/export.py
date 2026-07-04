from __future__ import annotations
from dataclasses import dataclass
import bpy
import os
import re
import math

CHUNK_SIZE = 250.0

BONE_MASKS = {
    "action": ["DEF-spine.001", "MCH-spine.002", "tag_weapon"]
}

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
    bone_indices: tuple[int, int, int, int] | None
    bone_weights: tuple[float, float, float, float] | None

    def __init__(self, position, normal, uv, color=None, bone_indices=None, bone_weights=None):
        self.position = tuple(round(x, 6) for x in position)
        self.normal = tuple(round(x, 6) for x in normal)
        self.uv = tuple(round(x, 6) for x in uv)
        self.color = tuple(round(c, 3) for c in color) if color else None
        self.bone_indices = bone_indices
        self.bone_weights = tuple(round(w, 5) for w in bone_weights) if bone_weights else None

    def __hash__(self):
        return hash((self.position, self.normal, self.uv, self.color, self.bone_indices, self.bone_weights))
    
    def __eq__(self, other):
        return (self.position, self.normal, self.uv, self.color, self.bone_indices, self.bone_weights) == (other.position, other.normal, other.uv, other.color, other.bone_indices, other.bone_weights)

class Surface:
    tris: list[tuple[int, int, int]]
    texture: str
    twosided: bool
    ocolor: bool
    ocolor_mult: bool
    multicolor: bool
    blend: str|None
    unlit: bool
    pm: str|None

    def __init__(self, name: str):
        self.tris = []
        self.texture = name
        self.twosided = False
        self.ocolor = False
        self.ocolor_mult = False
        self.blend = None
        self.unlit = False
        self.pm = None

class Model:
    skeleton: Skeleton|None
    vertices: list[Vertex]
    vertex_map: dict[Vertex, int]
    materials: dict[str, Surface]
    make_col_trimesh: bool
    make_convex_hull: bool
    cols: list[tuple[str, Transform, float, float]]
    col_pm: str|None
    centerofmass: tuple[float, float, float]|None
    params: list[tuple[str, str]]
    locations: list[tuple[str, Transform]]

    def __init__(self, skeleton=None):
        self.skeleton = skeleton
        self.vertices = []
        self.vertex_map = {}
        self.materials = {}
        self.make_col_trimesh = False
        self.make_convex_hull = False
        self.cols = []
        self.col_pm = None
        self.centerofmass = None
        self.params = []
        self.locations = []

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
    static_models: list
    static_models_idx: dict[str, int]
    static_objects: list[tuple[int, Transform]]
    graphs: list[Graph]
    chunks: dict[tuple[int, int], Chunk]
    max_chunk: tuple[int, int]
    locations: list[tuple[str, Transform]]

    def __init__(self):
        self.basemodel = Model()
        self.basemodel_name = ""
        self.static_models = []
        self.static_models_idx = {}
        self.static_objects = []
        self.graphs = []
        self.chunks = None
        self.max_chunk = (0, 0)
        self.locations = []

    def get_static_model_idx(self, name: str) -> int:
        if name in self.static_models_idx:
            return self.static_models_idx[name]
        
        idx = len(self.static_models)
        self.static_models.append(name)
        self.static_models_idx[name] = idx

        return idx

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

            idx = self.get_static_model_idx(name)

            chunk = get_chunk(pos_to_chunk(pos))
            chunk.extend_aabb(pos)
            chunk.static_objects.append((idx, trans))

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
    locations: list[tuple[str, Transform]]

    def __init__(self):
        self.wheels = []
        self.locations = []

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
    cyclic: bool

    def __init__(self, name: str, fps: float, frames: int):
        self.name = name
        self.fps = fps
        self.frames = frames
        self.channels = []
        self.cyclic = False

class HitBone:
    name: str
    bone: str
    shape: str
    transform: Transform
    sy: float
    sz: float
    
    def __init__(self, name: str, bone: str, shape: str, transform: Transform, sy: float, sz: float):
        self.name = name
        self.bone = bone
        self.shape = shape
        self.transform = transform
        self.sy = sy
        self.sz = sz

class Skeleton:
    name: str
    bones: list[Bone]
    bone_indices: dict[str, int]
    anims: list[Animation]
    hitbones: list[HitBone]
    locations: list[tuple[str, str, Transform]]

    def __init__(self, name, armature):
        self.name = name
        self.bones = []
        self.bone_indices = {}
        self.armature = armature
        self.anims = []
        self.hitbones = []
        self.locations = []

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

                bone_indices = None
                bone_weights = None

                if model.skeleton is not None:
                    deform_bones = []

                    for vert_group in vertex.groups:
                        weight = round(vert_group.weight, 3)

                        if weight < 0.001:
                            continue

                        group = obj.vertex_groups[vert_group.group]

                        if group.name not in model.skeleton.bone_indices:
                            continue

                        deform_bones.append((group.name, weight))

                    deform_bones.sort(key=lambda x: x[1], reverse=True)
                    deform_bones = deform_bones[:4]
                    weight_sum = sum(w for _, w in deform_bones)

                    bone_indices = tuple(model.skeleton.bone_indices[name] for name, _ in deform_bones)
                    bone_weights = tuple(w / weight_sum for _, w in deform_bones)

                vert = Vertex(position=pos, normal=normal, uv=uv, color=color, bone_indices=bone_indices, bone_weights=bone_weights)
                vert_index = model.add_vertex(vert)

                face_indices.append(vert_index)

            if mat_name not in model.materials:
                surface = Surface(mat_name)
                surface.twosided = "2S" in mat_params
                surface.ocolor = "OCOLOR" in mat_params
                surface.ocolor_mult = "OCOLOR_MULT" in mat_params
                surface.multicolor = "MULTICOLOR" in mat_params
                surface.unlit = "UNLIT" in mat_params
                
                blend = mat_params.get("BLEND")
                if isinstance(blend, str):
                    surface.blend = blend

                texture = mat_params.get("T")
                if isinstance(texture, str):
                    surface.texture = texture

                pm = mat_params.get("PM")
                if isinstance(pm, str):
                    surface.pm = pm

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

            if model.col_pm is not None:
                f.write(f"cpm {model.col_pm}\n")

            if model.centerofmass is not None:
                x, y, z = model.centerofmass
                f.write(f"centerofmass {x:.3f} {y:.3f} {z:.3f}\n")

            for col in model.cols:
                coltype, trans, sy, sz = col
                f.write(f"col {coltype} {self.transform_str(trans)} {sy:.6f} {sz:.6f}\n")

            if model.skeleton is not None:
                f.write(f"skeleton {model.skeleton.name}\n")

            for param_name, param_value in model.params:
                f.write(f"param {param_name} {param_value}\n")

            for loc_name, loc_trans in model.locations:
                f.write(f"loc {loc_name} {self.transform_str(loc_trans)}\n")

            for v in model.vertices:
                color_str = f" {v.color[0]} {v.color[1]} {v.color[2]}" if v.color else ""
                bones_str = f" {len(v.bone_indices)} " + " ".join(f"{i} {w}" for i, w in zip(v.bone_indices, v.bone_weights)) if model.skeleton is not None else ""
                f.write(f"v {v.position[0]} {v.position[1]} {v.position[2]} {v.normal[0]} {v.normal[1]} {v.normal[2]} {v.uv[0]} {v.uv[1]}{color_str}{bones_str}\n")
            
            for mat_name, surface in model.materials.items():
                f.write(f"surface {mat_name} +texture {surface.texture}")
                if surface.twosided:
                    f.write(" +2sided")
                if surface.ocolor:
                    f.write(" +ocolor")
                if surface.ocolor_mult:
                    f.write(" +ocolor_mult")
                if surface.multicolor:
                    f.write(" +multicolor")
                if surface.unlit:
                    f.write(" +unlit")
                if surface.blend is not None:
                    f.write(f" +blend {surface.blend}")
                f.write("\n")

                pm = surface.pm if surface.pm is not None else "none"
                f.write(f"pm {pm}\n")

                for tri in surface.tris:
                    f.write(f"f {tri[0]} {tri[1]} {tri[2]}\n")
    
    def export_map(self, map: Map, filepath: str):
        with open(filepath, "w") as f:
            f.write(f"basemodel {map.basemodel_name}\n")
            
            # static models
            for name in map.static_models:
                f.write(f"model {name}\n")

            f.write(f"endmodels\n")

            # graphs
            for graph in map.graphs:
                f.write(f"graph {graph.name}\n")
                for node in graph.nodes:
                    f.write(f"n {node.pos[0]} {node.pos[1]} {node.pos[2]}\n")
                for edge in graph.edges:
                    f.write(f"e {edge.nodes[0]} {edge.nodes[1]}\n")

            # locations
            for name, trans in map.locations:
                f.write(f"loc {name} {self.transform_str(trans)}\n")

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

                for model_idx, transform in chunk.static_objects:
                    f.write(f"static {model_idx} {self.transform_str(transform)}\n")

    def export_veh(self, veh: Vehicle, filepath: str):
        with open(filepath, "w") as f:
            f.write(f"basemodel {veh.basemodel_name}\n")
            
            for wheel in veh.wheels:
                f.write(f"wheel {wheel.type} {wheel.model_name} {wheel.position[0]} {wheel.position[1]} {wheel.position[2]} {wheel.radius}\n")
    
            for name, trans in veh.locations:
                f.write(f"loc {name} {self.transform_str(trans)}\n")


    def export_sk(self, sk: Skeleton, filepath: str):
        with open(filepath, "w") as f:
            for bone in sk.bones:
                parent_str = bone.parent if bone.parent is not None else "NONE"
                f.write(f"b {bone.name} {parent_str} {self.transform_str(bone.trans)}\n")

            for hitbone in sk.hitbones:
                f.write(f"hitbone {hitbone.name} {hitbone.bone} {hitbone.shape} {self.transform_str(hitbone.transform)} {hitbone.sy:.6f} {hitbone.sz:.6f}\n")

            for loc, bone_name, trans in sk.locations:
                f.write(f"loc {loc} {bone_name} {self.transform_str(trans)}\n")

            for anim in sk.anims:
                f.write(f"anim {anim.name} {sk.name}_{anim.name}\n")
                
    def export_anim(self, anim: Animation, filepath: str):
        with open(filepath, 'w') as f:
            f.write(f"frames {anim.frames}\n")
            f.write(f"fps {anim.fps}\n")

            if anim.cyclic:
                f.write("cyclic\n")

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

        if "CPM" in params:
            model.col_pm = params["CPM"]

        for obj in col.objects:
            type, obj_name, obj_params = self.extract_name(obj.name)

            if type == "M":
                self.add_mesh_to_model(obj, model)
            elif type == "COL":
                t, r, s = obj.matrix_world.decompose()
                trans = Transform((t.x, t.y, t.z), (r.x, r.y, r.z, r.w), s.x)

                if obj_name == "box":
                    model.cols.append(("box", trans, s.y, s.z))
            elif type == "COM":
                trans = self.get_obj_transform(obj)
                model.centerofmass = trans.position
            elif type == "P":
                if not "V" in obj_params:
                    continue
                model.params.append((obj_name, obj_params["V"]))
            elif type == "LOC":
                trans = self.get_obj_transform(obj)
                model.locations.append((obj_name, trans))

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
                elif type == "LOC":
                    map.locations.append((obj_name, self.get_obj_transform(obj)))

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

            elif type == "LOC":
                transform = self.get_obj_transform(obj)
                veh.locations.append((obj_name, transform))

        veh.wheels.sort(key=lambda w: w.type)
        veh.locations.sort(key=lambda l: l[0])

        veh_filepath = os.path.join(self.out_path, f"{name}.veh")
        self.export_veh(veh, veh_filepath)

    def get_armature_keep_bones(self, armature):
        keep_bones = set()
        
        for bone in armature.data.bones:
            #bone_name = bone.name

            if bone.use_deform or bone.name.startswith("tag_"): # or is_tag:
                keep_bones.add(bone)

                while bone.parent:
                    bone = bone.parent
                    keep_bones.add(bone)

        return keep_bones
    
    def get_masked_bones(self, mask: str|None, skeleton: Skeleton) -> set[str]:
        if mask is None:
            return set(bonename for bonename in skeleton.bone_indices)
        
        parent_list = BONE_MASKS[mask]
        bones = set()

        for bonename in skeleton.bone_indices:
            parentname: str|None = bonename
            while True:
                if parentname in parent_list:
                    bones.add(bonename)
                    break

                parentname = skeleton.bones[skeleton.bone_indices[parentname]].parent

                if parentname is None:
                    break

        return bones

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
            sk.bone_indices[xbone.name] = len(sk.bones) - 1

        self.skeletons[name] = sk

        # export meshes
        for child in obj.children:
            type, obj_name, child_params = self.extract_name(child.name)

            if type == "SKM":
                model = Model(sk)
                self.add_mesh_to_model(child, model)
                mdl_filepath = os.path.join(self.out_path, f"{obj_name}.mdl")
                self.export_mdl(model, mdl_filepath)
            elif type == "HIT":
                bone_name = child.parent_bone
                pose_bone = obj.pose.bones[bone_name]
                bone_world = obj.matrix_world @ pose_bone.matrix
                obj_world = child.matrix_world
                relative_matrix = bone_world.inverted() @ obj_world
                
                t, r, s = relative_matrix.decompose()
                trans = Transform((t.x, t.y, t.z), (r.x, r.y, r.z, r.w), s.x)
                shape = child_params["S"] if "S" in child_params else "capsule"
                sk.hitbones.append(HitBone(obj_name, bone_name, shape, trans, s.y, s.z))

            elif type == "LOC":
                bone_name = child.parent_bone
                pose_bone = obj.pose.bones[bone_name]
                bone_world = obj.matrix_world @ pose_bone.matrix
                obj_world = child.matrix_world
                relative_matrix = bone_world.inverted() @ obj_world
                
                t, r, s = relative_matrix.decompose()
                trans = Transform((t.x, t.y, t.z), (r.x, r.y, r.z, r.w), s.x)
                sk.locations.append((obj_name, bone_name, trans))

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

        anim_bones = self.get_masked_bones(params["M"] if "M" in params else None, sk)

        bone_frames = {bonename: [] for bonename in anim_bones}

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

        cyclic = "C" in params
        numframes = end if cyclic else end + 1

        for frame in range(0, numframes):
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()

            for bonename in anim_bones:
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

        anim = Animation(anim_name, fps, numframes)
        anim.cyclic = cyclic

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
