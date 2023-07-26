#pragma once

#include "glm/glm.hpp"
#include "nlohmann/json.hpp"
#include "StreamReader.hpp"
#include "File.hpp"
#include <vector>
#include <iostream>
#include <unordered_set>
#include <filesystem>

class Prm
{
public:
#pragma pack(push, 1)
    struct Index
    {
        uint16_t a = 0;
        uint16_t b = 0;
        uint16_t c = 0;
    };

    struct Mesh
    {
        uint8_t lod = 0;
        uint16_t material_id = 0;
        int32_t diffuse_id = -1;
        int32_t normal_id = -1;
        int32_t specular_id = -1;
        std::string material_dir = "";
        std::string material_file = "";
        std::string diffuse_dir = "";
        std::string diffuse_file = "";
        std::string normal_dir = "";
        std::string normal_file = "";
        std::string specular_dir = "";
        std::string specular_file = "";
        std::vector<glm::vec3> vertices;
        std::vector<Index> indices;
        std::vector<glm::vec2> uvs;
    };

    struct Model
    {
        uint32_t chunk = 0;
        std::vector<Mesh> meshes;
    };

    struct Chunk
    {
        std::vector<char> data;
        bool is_model = false;
        uint32_t model = 0;
    };

    struct Entry
    {
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t type = 0;
        uint32_t pad = 0;
    };
    
    struct Header
    {
        uint32_t table_offset = 0;
        uint32_t table_count = 0;
    };

    struct PrmFile
    {
        Header header;
        std::vector<Entry> entries;
        std::vector<Chunk> chunks;
        std::vector<Model> models;
    };
#pragma pack(pop)

    Prm() = default;

    void Load(char* data, uint32_t size)
    {
        StreamReader prm_stream(data, size);
        prm_stream.Read<Header>(&prm.header);
        prm_stream.SeekTo(prm.header.table_offset);
        prm.entries.resize(prm.header.table_count);
        prm_stream.Read<Entry>(prm.entries.data(), prm.header.table_count);
        prm.chunks.resize(prm.header.table_count);
        //std::cout << "1" << std::endl;
        for (uint32_t i = 0; i < prm.entries.size(); i++)
        {
            prm_stream.SeekTo(prm.entries[i].offset);
            prm.chunks[i].data.resize(prm.entries[i].size);
            prm_stream.Read<char>(prm.chunks[i].data.data(), prm.entries[i].size);
            if (prm.entries[i].size == 0x40)
            {
                if (*reinterpret_cast<uint32_t*>(prm.chunks[i].data.data()) == 0x70100)
                {
                    prm.chunks[i].is_model = true;
                    prm.chunks[i].model = (uint32_t)prm.models.size();
                    prm.models.emplace_back();
                    prm.models.back().chunk = i;
                }
            }
            //std::cout << "2" << std::endl;
        }
        
        for (auto& model : prm.models)
        {
            //std::cout << "prm.entries[i].offset " << prm.entries[model.chunk].offset << std::endl;
            StreamReader model_stream(prm.chunks[model.chunk].data.data(), prm.chunks[model.chunk].data.size());
            //std::cout << "model stream size " << prm.chunks[model.chunk].data.size() << std::endl;
            model_stream.SeekTo(0x14);
            uint32_t mesh_count = 0;
            uint32_t mesh_table = 0;
            model_stream.Read<uint32_t>(&mesh_count);
            model_stream.Read<uint32_t>(&mesh_table);
            //std::cout << "mesh_count " << mesh_count << std::endl;
            //std::cout << "mesh_table " << mesh_table << std::endl;
            StreamReader mesh_table_stream(prm.chunks[mesh_table].data.data(), prm.chunks[mesh_table].data.size());
            for (uint32_t i = 0; i < mesh_count; i++)
            {
                uint32_t mesh_chunk = 0;
                mesh_table_stream.Read<uint32_t>(&mesh_chunk);
                //std::cout << "mesh_chunk " << mesh_chunk << std::endl;
                StreamReader mesh_stream(prm.chunks[mesh_chunk].data.data(), prm.chunks[mesh_chunk].data.size());
                //std::cout << "mesh_stream size " << prm.chunks[mesh_chunk].data.size() << std::endl;
                mesh_stream.SeekTo(0xE);
                uint8_t lod = 0;
                mesh_stream.Read<uint8_t>(&lod);
                if (lod & (uint8_t)1 == (uint8_t)1)
                {
                    model.meshes.emplace_back();
                    model.meshes.back().lod = lod;
                    //std::cout << "model.meshes.back().lod " << model.meshes.back().lod << std::endl;
                    mesh_stream.SeekBy(3);
                    mesh_stream.Read<uint16_t>(&model.meshes.back().material_id);
                    //std::cout << "model.meshes.back().material_id " << model.meshes.back().material_id << std::endl;
                    mesh_stream.SeekBy(0x14);
                    uint32_t mesh_desc_pointer_chunk = 0;
                    mesh_stream.Read<uint32_t>(&mesh_desc_pointer_chunk);
                    //std::cout << "mesh_desc_pointer_chunk " << mesh_desc_pointer_chunk << std::endl;
                    if (mesh_desc_pointer_chunk >= prm.chunks.size())
                    {
                        continue;
                    }
                    //std::cout << "mesh_desc_pointer_chunk offset " << prm.entries[mesh_desc_pointer_chunk].offset << std::endl;
                    //std::cout << "passed " << std::endl;
                    StreamReader mesh_desc_pointer_stream(prm.chunks[mesh_desc_pointer_chunk].data.data(), prm.chunks[mesh_desc_pointer_chunk].data.size());
                    uint32_t mesh_desc_chunk = 0;
                    mesh_desc_pointer_stream.Read<uint32_t>(&mesh_desc_chunk);
                    //std::cout << "mesh_desc_chunk " << mesh_desc_chunk << std::endl;
                    if (mesh_desc_chunk >= prm.chunks.size())
                    {
                        continue;
                    }
                    //std::cout << "mesh_desc_chunk offset " << prm.entries[mesh_desc_chunk].offset << std::endl;
                    StreamReader mesh_desc_stream(prm.chunks[mesh_desc_chunk].data.data(), prm.chunks[mesh_desc_chunk].data.size());
                    uint32_t vertex_num = 0;
                    mesh_desc_stream.Read<uint32_t>(&vertex_num);
                    //std::cout << "vertex_num " << vertex_num << std::endl;
                    uint32_t vertices_chunk = 0;
                    mesh_desc_stream.Read<uint32_t>(&vertices_chunk);
                    //std::cout << "vertices_chunk " << vertices_chunk << std::endl;
                    mesh_desc_stream.SeekBy(4);
                    uint32_t triangles_chunk = 0;
                    mesh_desc_stream.Read<uint32_t>(&triangles_chunk);
                    //std::cout << "triangles_chunk " << triangles_chunk << std::endl;
                    //std::cout << "triangles_chunk offset " << prm.entries[triangles_chunk].offset << std::endl;
                    if (vertex_num != 0 && vertices_chunk != 0 && triangles_chunk != 0)
                    {
                        StreamReader triangles_stream(prm.chunks[triangles_chunk].data.data(), prm.chunks[triangles_chunk].data.size());
                        triangles_stream.SeekBy(2);
                        uint16_t triangles_num = 0;
                        triangles_stream.Read<uint16_t>(&triangles_num);
                        uint32_t vertices_size = (uint32_t)prm.chunks[vertices_chunk].data.size() / vertex_num;
                        vertices_size -= vertices_size % 4;
                        if (vertices_size != 0x28 && vertices_size % 0x28 == 0)
                        {
                            vertex_num *= vertices_size / 0x28;
                            vertices_size = 0x28;
                        }
                        StreamReader vertices_stream(prm.chunks[vertices_chunk].data.data(), prm.chunks[vertices_chunk].data.size());
                        if (vertices_size == 0x10)
                        {
                            for (uint32_t j = 0; j < vertex_num; j++)
                            {
                                model.meshes.back().vertices.emplace_back();
                                vertices_stream.Read<glm::vec3>(&model.meshes.back().vertices.back());
                                vertices_stream.SeekBy(4);
                            }
                            for (uint32_t j = 0; j < triangles_num / 3; j++)
                            {
                                model.meshes.back().indices.emplace_back();
                                triangles_stream.Read<Index>(&model.meshes.back().indices.back());
                            }
                        }
                        else if (vertices_size == 0x24)
                        {
                            for (uint32_t j = 0; j < vertex_num; j++)
                            {
                                model.meshes.back().vertices.emplace_back();
                                vertices_stream.Read<glm::vec3>(&model.meshes.back().vertices.back());                            
                                vertices_stream.SeekBy(0x10);
                                model.meshes.back().uvs.emplace_back();
                                glm::vec2 uv;
                                vertices_stream.Read<glm::vec2>(&uv);
                                model.meshes.back().uvs.back().x = uv.x;
                                model.meshes.back().uvs.back().y = 1.0 - uv.y;
                            }
                            for (uint32_t j = 0; j < triangles_num / 3; j++)
                            {
                                model.meshes.back().indices.emplace_back();
                                triangles_stream.Read<Index>(&model.meshes.back().indices.back());
                            }
                        }
                        else if (vertices_size == 0x28)
                        {
                            for (uint32_t j = 0; j < vertex_num; j++)
                            {
                                model.meshes.back().vertices.emplace_back();
                                vertices_stream.Read<glm::vec3>(&model.meshes.back().vertices.back());                            
                                vertices_stream.SeekBy(8);
                                model.meshes.back().uvs.emplace_back();
                                glm::vec2 uv;
                                vertices_stream.Read<glm::vec2>(&uv);
                                model.meshes.back().uvs.back().x = uv.x;
                                model.meshes.back().uvs.back().y = 1.0 - uv.y;
                                vertices_stream.SeekBy(0xC);
                            }
                            for (uint32_t j = 0; j < triangles_num / 3; j++)
                            {
                                model.meshes.back().indices.emplace_back();
                                triangles_stream.Read<Index>(&model.meshes.back().indices.back());
                            }
                        }
                        else if (vertices_size == 0x34)
                        {
                            for (uint32_t j = 0; j < vertex_num; j++)
                            {
                                model.meshes.back().vertices.emplace_back();
                                vertices_stream.Read<glm::vec3>(&model.meshes.back().vertices.back());                            
                                vertices_stream.SeekBy(0x18);
                                model.meshes.back().uvs.emplace_back();
                                glm::vec2 uv;
                                vertices_stream.Read<glm::vec2>(&uv);
                                model.meshes.back().uvs.back().x = uv.x;
                                model.meshes.back().uvs.back().y = 1.0 - uv.y;
                                vertices_stream.SeekBy(8);
                            }
                            for (uint32_t j = 0; j < triangles_num / 3; j++)
                            {
                                model.meshes.back().indices.emplace_back();
                                triangles_stream.Read<Index>(&model.meshes.back().indices.back());
                            }
                        }
                    }
                }
            }
        }  
    }

    PrmFile& GetPrm()
    {
        return prm;
    }

    void GetModelJson(uint32_t index, nlohmann::ordered_json& json, nlohmann::ordered_json& mat_json, nlohmann::ordered_json& tex_json)
    {
        if (index < prm.chunks.size())
        {
            if (prm.chunks[index].is_model)
            {
                json["Model Id"] = prm.chunks[index].model;
                nlohmann::ordered_json meshes = nlohmann::ordered_json::array();
                for (auto& mesh : prm.models[prm.chunks[index].model].meshes)
                {
                    nlohmann::ordered_json mesh_json;
                    mesh_json["LOD"] = mesh.lod;
                    mesh_json["Material Id"] = mesh.material_id;
                    if (mat_json["Entries"][mesh.material_id - 1].contains("Instance"))
                    {
                        ParseMaterialPath(mesh.material_id, mat_json["Entries"][mesh.material_id - 1]["Instance"][0]["Name"], mesh.material_dir, mesh.material_file);
                        mesh_json["Material Path"] = mesh.material_dir;
                        mesh_json["Material File"] = mesh.material_file;
                        if (mat_json["Entries"][mesh.material_id - 1]["Instance"][0].contains("Binder"))
                        {
                            if (mat_json["Entries"][mesh.material_id - 1]["Instance"][0]["Binder"][0].contains("Texture"))
                            {
                                for (auto& texture : mat_json["Entries"][mesh.material_id - 1]["Instance"][0]["Binder"][0]["Texture"].items())
                                {
                                    if (texture.value()["Name"] == "mapDiffuse")
                                    {
                                        if (texture.value().contains("Texture Id"))
                                        {
                                            mesh.diffuse_id = texture.value()["Texture Id"];
                                            std::string key = std::to_string(mesh.diffuse_id);
                                            if (tex_json.contains(key))
                                            {
                                                ParseTexturePath(mesh.diffuse_id, tex_json[key], mesh.diffuse_dir, mesh.diffuse_file);
                                                mesh_json["Diffuse Path"] = mesh.diffuse_dir;
                                                mesh_json["Diffuse File"] = mesh.diffuse_file;
                                            }
                                        }
                                    }
                                    else if (texture.value()["Name"] == "mapNormal")
                                    {
                                        if (texture.value().contains("Texture Id"))
                                        {
                                            mesh.normal_id = texture.value()["Texture Id"];
                                            std::string key = std::to_string(mesh.normal_id);
                                            if (tex_json.contains(key))
                                            {
                                                ParseTexturePath(mesh.normal_id, tex_json[key], mesh.normal_dir, mesh.normal_file);
                                                mesh_json["Normal Path"] = mesh.normal_dir;
                                                mesh_json["Normal File"] = mesh.normal_file;
                                            }
                                        }
                                    }
                                    else if (texture.value()["Name"] == "mapSpecularMask")
                                    {
                                        if (texture.value().contains("Texture Id"))
                                        {
                                            mesh.specular_id = texture.value()["Texture Id"];
                                            std::string key = std::to_string(mesh.specular_id);
                                            if (tex_json.contains(key))
                                            {
                                                ParseTexturePath(mesh.specular_id, tex_json[key], mesh.specular_dir, mesh.specular_file);
                                                mesh_json["Specular Path"] = mesh.specular_dir;
                                                mesh_json["Specular File"] = mesh.specular_file;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    mesh_json["Diffuse Id"] = mesh.diffuse_id;
                    mesh_json["Normal Id"] = mesh.normal_id;
                    mesh_json["Specular Id"] = mesh.specular_id;
                    meshes.push_back(mesh_json);
                }
                if (meshes.size() > 0)
                {
                    json["Meshes"] = meshes;
                }
            }
        }
    }

    void GenerateJson(std::string& json_string)
    {
        nlohmann::ordered_json json;

        json["Models"] = nlohmann::ordered_json::array();

        for (auto& model : prm.models)
        {
            nlohmann::ordered_json model_json;
            model_json["Chunk"] = model.chunk;
            model_json["Meshes"] = nlohmann::ordered_json::array();
            for (auto& mesh : model.meshes)
            {
                nlohmann::ordered_json mesh_json;
                mesh_json["LOD"] = mesh.lod;
                mesh_json["Material Id"] = mesh.material_id;
                model_json["Meshes"].push_back(mesh_json);
            }
            json["Models"].push_back(model_json);
        }
        
        json_string = json.dump();
    }

    void WriteObj(std::string output_path, uint32_t primitive_id, uint32_t* sub_meshes, uint32_t sub_meshes_count)
    {
        std::string obj = "";
        uint32_t face_index = 1;
        std::unordered_set<std::string> materials;
        uint32_t mesh_index = 0;
        for (auto& mesh : prm.models[primitive_id].meshes)
        {
            bool export_mesh = true;
            for (uint32_t m = 0; m < sub_meshes_count; m++)
            {
                //std::cout << "m: " << m << ", sub_meshes[m]: " << sub_meshes[m] << std::endl;
                if (mesh_index == sub_meshes[m])
                {
                    export_mesh = false;
                }
            }
            //std::cout << "mesh_index: " << mesh_index << ", export_mesh " << export_mesh << std::endl;
            if (export_mesh)
            {
                uint32_t face_count = 0;
                if (mesh.uvs.size() == 0)
                {
                    for (auto& vertex : mesh.vertices)
                    {
                        obj += "v ";
                        obj += std::to_string(vertex.x);
                        obj += " ";
                        obj += std::to_string(vertex.y);
                        obj += " ";
                        obj += std::to_string(vertex.z);
                        obj += "\n";
                        face_count++;
                    }
                    obj += "s 1\n";
                    for (auto& index : mesh.indices)
                    {
                        obj += "f ";
                        obj += std::to_string(index.a + face_index);
                        obj += " ";
                        obj += std::to_string(index.b + face_index);
                        obj += " ";
                        obj += std::to_string(index.c + face_index);
                        obj += "\n";
                    }
                }
                else
                {
                    if (mesh.diffuse_id >= 0 || mesh.normal_id >= 0 || mesh.specular_id >= 0)
                    {
                        for (auto& vertex : mesh.vertices)
                        {
                            obj += "v ";
                            obj += std::to_string(vertex.x);
                            obj += " ";
                            obj += std::to_string(vertex.y);
                            obj += " ";
                            obj += std::to_string(vertex.z);
                            obj += "\n";
                            face_count++;
                        }
                        for (auto& uv : mesh.uvs)
                        {
                            obj += "vt ";
                            obj += std::to_string(uv.x);
                            obj += " ";
                            obj += std::to_string(uv.y);
                            obj += "\n";
                        }
                        std::string material_name = "mat_";
                        material_name += std::to_string(mesh.material_id);
                        if (!materials.contains(material_name))
                        {
                            std::string material_output_path = output_path;
                            material_output_path += mesh.material_dir;
                            if (!std::filesystem::exists(material_output_path))
                            {
                                if (!std::filesystem::create_directories(material_output_path))
                                {
                                    std::cout << "Error: Could not create directory " << material_output_path << std::endl;
                                }
                            }
                            std::string nested = "../";
                            for (uint32_t i = 0; i < mesh.material_dir.length(); i++)
                            {
                                if (mesh.material_dir[i] == '/')
                                {
                                    nested += "../";
                                }
                            }
                            std::string mtl = "";
                            obj += "mtllib ../";
                            obj += mesh.material_dir;
                            obj += "/";
                            obj += mesh.material_file;
                            obj += ".mtl\n";
                            mtl += "newmtl ";
                            mtl += mesh.material_file;
                            mtl += "\nKa 1.000 1.000 1.000\nKd 1.000 1.000 1.000\nKs 0.000 0.000 0.000\nNs 1.0\nd 1.0\nillum 2\n";
                            if (mesh.diffuse_id >= 0)
                            {
                                mtl += "map_Kd ";
                                mtl += nested;
                                mtl += mesh.diffuse_dir;
                                mtl += "/";
                                mtl += mesh.diffuse_file;
                                mtl += "\n";
                            }
                            if (mesh.specular_id >= 0)
                            {
                                mtl += "map_Ks ";
                                mtl += nested;
                                mtl += mesh.specular_dir;
                                mtl += "/";
                                mtl += mesh.specular_file;
                                mtl += "\n";
                            }
                            if (mesh.normal_id >= 0)
                            {
                                mtl += "map_Bump ";
                                mtl += nested;
                                mtl += mesh.normal_dir;
                                mtl += "/";
                                mtl += mesh.normal_file;
                                mtl += "\n";
                            }
                            material_output_path += "/";
                            material_output_path += mesh.material_file;
                            material_output_path += ".mtl";
                            File::WriteToFile(material_output_path, mtl);
                        }
                        obj += "usemtl ";
                        obj += mesh.material_file;
                        obj += "\n";
                        obj += "s 1\n";
                        for (auto& index : mesh.indices)
                        {
                            obj += "f ";
                            obj += std::to_string(index.a + face_index);
                            obj += "/";
                            obj += std::to_string(index.a + face_index);
                            obj += " ";
                            obj += std::to_string(index.b + face_index);
                            obj += "/";
                            obj += std::to_string(index.b + face_index);
                            obj += " ";
                            obj += std::to_string(index.c + face_index);
                            obj += "/";
                            obj += std::to_string(index.c + face_index);
                            obj += "\n";
                        }
                    }
                }
                face_index += face_count;
            }
            mesh_index++;
        }
        std::string obj_output_path = output_path;    
        obj_output_path += "Models";    
        if (!std::filesystem::exists(obj_output_path))
        {
            if (!std::filesystem::create_directories(obj_output_path))
            {
                std::cout << "Error: Could not create directory " << obj_output_path << std::endl;
            }
        }
        obj_output_path += "/";
        obj_output_path += std::to_string(primitive_id);
        obj_output_path += ".obj";
        File::WriteToFile(obj_output_path, obj);
    }

    void ParseMaterialPath(uint32_t index, std::string input, std::string& dir, std::string& file)
    {
        size_t pos = input.rfind('/');
        if (pos != std::string::npos)
        {
            dir = "Materials/";
            dir += input.substr(0, pos);
            file = std::to_string(index);
            file += "-";
            file += input.substr(pos + 1);
        }
        else
        {
            dir = "Materials";
            file = std::to_string(index);
            file += "-";
            file += input;
        }
    }

    void ParseTexturePath(uint32_t index, std::string input, std::string& dir, std::string& file)
    {
        size_t pos = input.rfind('/');
        if (pos != std::string::npos)
        {
            dir = "Textures/";
            dir += input.substr(0, pos);
            file = std::to_string(index);
            file += "-";
            file += input.substr(pos + 1);
            file += ".tga";
        }
        else
        {
            dir = "Textures";
            file = std::to_string(index);
            file += "-";
            file += input;
            file += ".tga";
        }
    }

private:
    PrmFile prm;
};