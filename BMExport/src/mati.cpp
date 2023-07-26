#include "mati.h"
#include "util.h"
#include "File.hpp"
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <typeinfo>

mati::mati() = default;

void mati::load(char* data, uint32_t size)
{
    mati_data = std::vector<char>(data, data + size);
}

void mati::generate_json(std::string& json_string)
{
    nlohmann::ordered_json json;
    generate_json(json);
    json_string = json.dump();
}

void mati::generate_json(nlohmann::ordered_json& json)
{
    uint32_t position = 0;
    uint8_t bytes1 = 0;
    uint32_t bytes4 = 0;
    uint64_t bytes8 = 0;

    uint32_t header_offset1 = 0;
    std::memcpy(&header_offset1, &mati_data.data()[position], sizeof(bytes4));
    position += 4;
    uint32_t header_offset2 = 0;
    std::memcpy(&header_offset2, &mati_data.data()[position], sizeof(bytes4));
    int index = 1;
    
    json["Classes"] = nlohmann::ordered_json::array();

    while (true)
    {
        position = header_offset1;
        position += index * 4;

        std::memcpy(&header_offset, &mati_data.data()[position], sizeof(bytes4));
        position = header_offset;

        if (position == 0)
        {
            break;
        }

        std::memcpy(&type_offset, &mati_data.data()[position], sizeof(bytes4));
        position += 4;

        type = std::string(&mati_data.data()[type_offset]);

        std::memcpy(&texture_count, &mati_data.data()[position], sizeof(bytes4));
        position += 0x4;

        std::memcpy(&unknown_flag_1, &mati_data.data()[position], sizeof(bytes4));
        position += 0x4;

        std::memcpy(&unknown_flag_2, &mati_data.data()[position], sizeof(bytes4));
        position += 0x4;

        std::memcpy(&eres_index, &mati_data.data()[position], sizeof(bytes4));
        position += 0xC;

        std::memcpy(&instance_offset, &mati_data.data()[position], sizeof(bytes4));
        position = instance_offset;

        //std::cout << "MATI header_offset: " << util::uint32_t_to_hex_string(header_offset) << std::endl;
        //std::cout << "MATI type_offset: " << util::uint32_t_to_hex_string(type_offset) << std::endl;
        //std::cout << "MATI type: " << type << std::endl;
        //std::cout << "MATI texture_count: " << texture_count << std::endl;
        //std::cout << "MATI unknown_flag_1: " << unknown_flag_1 << std::endl;
        //std::cout << "MATI unknown_flag_2: " << unknown_flag_2 << std::endl;
        //std::cout << "MATI eres_index: " << eres_index << std::endl;
        //std::cout << "MATI instance_offset: " << util::uint32_t_to_hex_string(instance_offset) << std::endl;
        
        nlohmann::ordered_json element = nlohmann::ordered_json::object();
        element["Type"] = type;
        element = read_properties_json(element, position, -1, false);

        json["Classes"].push_back(element);

        index++;
    }
    
    index = 1;    
    json["Entries"] = nlohmann::ordered_json::array();

    while (true)
    {
        position = header_offset2;
        position += index * 4;

        std::memcpy(&header_offset, &mati_data.data()[position], sizeof(bytes4));
        position = header_offset;

        if (position == 0)
        {
            break;
        }

        std::memcpy(&type_offset, &mati_data.data()[position], sizeof(bytes4));
        position += 4;

        type = std::string(&mati_data.data()[type_offset]);

        std::memcpy(&texture_count, &mati_data.data()[position], sizeof(bytes4));
        position += 0x4;

        std::memcpy(&unknown_flag_1, &mati_data.data()[position], sizeof(bytes4));
        position += 0x4;

        std::memcpy(&unknown_flag_2, &mati_data.data()[position], sizeof(bytes4));
        position += 0x4;

        std::memcpy(&eres_index, &mati_data.data()[position], sizeof(bytes4));
        position += 0xC;

        std::memcpy(&instance_offset, &mati_data.data()[position], sizeof(bytes4));
        position = instance_offset;

        //std::cout << "MATI header_offset: " << util::uint32_t_to_hex_string(header_offset) << std::endl;
        //std::cout << "MATI type_offset: " << util::uint32_t_to_hex_string(type_offset) << std::endl;
        //std::cout << "MATI type: " << type << std::endl;
        //std::cout << "MATI texture_count: " << texture_count << std::endl;
        //std::cout << "MATI unknown_flag_1: " << unknown_flag_1 << std::endl;
        //std::cout << "MATI unknown_flag_2: " << unknown_flag_2 << std::endl;
        //std::cout << "MATI eres_index: " << eres_index << std::endl;
        //std::cout << "MATI instance_offset: " << util::uint32_t_to_hex_string(instance_offset) << std::endl;
        
        nlohmann::ordered_json element = nlohmann::ordered_json::object();
        element["Type"] = type;
        element = read_properties_json(element, position, -1, false);

        json["Entries"].push_back(element);

        index++;
    }
}

nlohmann::ordered_json
mati::read_properties_json(nlohmann::ordered_json temp_json, uint32_t position, uint32_t parent, bool from_mati_file)
{
    uint8_t bytes1 = 0;
    uint32_t bytes4 = 0;
    uint64_t bytes8 = 0;

    mati_property temp_mati_property;

    temp_mati_property.parent = parent;

    temp_mati_property.name[3] = mati_data.data()[position];
    position++;
    temp_mati_property.name[2] = mati_data.data()[position];
    position++;
    temp_mati_property.name[1] = mati_data.data()[position];
    position++;
    temp_mati_property.name[0] = mati_data.data()[position];
    position++;

    std::memcpy(&temp_mati_property.data, &mati_data.data()[position], sizeof(bytes4));
    position += 0x4;

    std::memcpy(&temp_mati_property.size, &mati_data.data()[position], sizeof(bytes4));
    position += 0x4;

    std::memcpy(&temp_mati_property.type, &mati_data.data()[position], sizeof(bytes4));
    position += 0x4;

    std::string mati_property_name = temp_mati_property.name;

    std::unordered_map<std::string, std::string>::iterator it = mati_property_name_map.find(
            temp_mati_property.name);

    if (it != mati_property_name_map.end())
    {
        //std::cout << "MATI temp_mati_property.name: " << temp_mati_property.name << " (" << it->second << ")" << std::endl;
        mati_property_name = it->second;
    }

    //std::cout << "MATI temp_mati_property.data: " << temp_mati_property.data << std::endl;
    //std::cout << "MATI temp_mati_property.size: " << temp_mati_property.size << std::endl;
    //std::cout << "MATI temp_mati_property.type: " << temp_mati_property.type << std::endl;

    if (temp_mati_property.type == 0)
    {
        //std::cout << "MATI temp_property_type: FLOAT" << std::endl;

        if (temp_mati_property.size == 1)
        {
            float temp_float = 0;

            std::memcpy(&temp_float, &temp_mati_property.data, sizeof(bytes4));

            temp_mati_property.float_values.push_back(temp_float);

            temp_json[mati_property_name] = temp_mati_property.float_values[0];
        } else {
            for (uint32_t i = 0; i < temp_mati_property.size; i++)
            {
                float temp_float = 0;

                std::memcpy(&temp_float, &mati_data.data()[temp_mati_property.data + i * 0x4], sizeof(bytes4));

                //std::cout << "MATI temp_mati_property.value (" << i << "): " << temp_float << std::endl;

                temp_mati_property.float_values.push_back(temp_float);
            }

            temp_json[mati_property_name] = temp_mati_property.float_values;
        }

        mati_properties.push_back(temp_mati_property);
    } else if (temp_mati_property.type == 1)
    {
        //std::cout << "MATI temp_property_type: STRING" << std::endl;

        temp_mati_property.string_value = std::string(&mati_data.data()[temp_mati_property.data]);

        //std::cout << "MATI temp_mati_property.value: " << temp_mati_property.string_value << std::endl;

        mati_properties.push_back(temp_mati_property);

        temp_json[mati_property_name] = temp_mati_property.string_value;
    } else if (temp_mati_property.type == 2)
    {
        //std::cout << "MATI temp_property_type: INT" << std::endl;

        temp_mati_property.int_value = temp_mati_property.data;

        mati_properties.push_back(temp_mati_property);

        temp_json[mati_property_name] = temp_mati_property.int_value;
    } else if (temp_mati_property.type == 3)
    {
        //std::cout << "MATI temp_property_type: PROPERTY" << std::endl;

        position = temp_mati_property.data;

        parent = mati_properties.size();

        nlohmann::ordered_json temp_json_array = nlohmann::ordered_json::array();

        nlohmann::ordered_json temp_json_object = nlohmann::ordered_json::object();

        for (uint32_t p = 0; p < temp_mati_property.size; p++)
        {
            temp_json_object = read_properties_json(temp_json_object, position, parent, from_mati_file);

            position += 0x10;
        }

        if (temp_json.contains(mati_property_name))
        {
            temp_json[mati_property_name].push_back(temp_json_object);
        } else {
            temp_json_array.push_back(temp_json_object);

            temp_json[mati_property_name] = temp_json_array;
        }
    }

    return temp_json;
}

void mati::align()
{
    while (mati_data.size() % 0x10 != 0)
    {
        mati_data.push_back(0x0);
    }
}

void mati::write_uint32_t(uint32_t data)
{
    char tmp_uint32_t[4];

    std::memcpy(&tmp_uint32_t, &data, 0x4);

    for (int i = 0; i < 0x4; i++)
    {
        mati_data.push_back(*((char*) &tmp_uint32_t + i));
    }
}

void mati::write_string(std::string data)
{
    for (int i = 0; i < data.length(); i++)
    {
        mati_data.push_back(data[i]);
    }

    mati_data.push_back(0x0);
}

void mati::write_float(float data)
{
    char tmp_float[4];

    std::memcpy(&tmp_float, &data, 0x4);

    for (int i = 0; i < 0x4; i++)
    {
        mati_data.push_back(*((char*) &tmp_float + i));
    }
}

void mati::write_name(std::string name)
{
    mati_data.push_back(name[3]);
    mati_data.push_back(name[2]);
    mati_data.push_back(name[1]);
    mati_data.push_back(name[0]);
}