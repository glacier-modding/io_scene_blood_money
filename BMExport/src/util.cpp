#include "util.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <regex>

bool util::is_valid_hash(const std::string& hash) {
    const std::string valid_chars = "0123456789ABCDEF";

    std::string hash_upper = util::to_upper_case(hash);

    if (hash_upper.length() != 16) {
        return false;
    }

    for (int i = 0; i < 16; i++) {
        if (valid_chars.find(hash_upper[i]) == std::string::npos) {
            return false;
        }
    }

    return true;
}

bool util::floats_equal(float value1, float value2) {
    if (value1 == 0.0f) {
        return value2 < 0.00000001f;
    }
    if (value2 == 0.0f) {
        return value1 < 0.00000001f;
    }
    if (std::fabs(value1 - value2) <= 0.00000001f) {
        return true;
    }
    return (std::fabs(value1 - value2) <= 1E-5 * std::fmax(std::fabs(value1), std::fabs(value2)));
}

std::string util::find_string_between_strings(std::string_view& input_string, const std::string& string_before,
                                              const std::string& string_after) {
    std::string output_string = "";

    const size_t pos1 = input_string.find(string_before);

    if (pos1 != std::string::npos) {
        const size_t position_after_string_before = pos1 + string_before.length();

        const size_t pos2 = input_string.substr(position_after_string_before).find(string_after);

        if (pos2 != std::string::npos) {
            output_string = input_string.substr(position_after_string_before, pos2);
        }
    }

    return output_string;
}

void util::split_string_view(const std::string_view& temp_string_view, const std::string& split_string,
                             std::vector<uint64_t>& split_positions) {
    bool done = false;

    uint64_t position = 0;

    while (!done) {
        const size_t pos1 = temp_string_view.substr(position).find(split_string);

        if (pos1 != std::string::npos) {
            split_positions.push_back(position + pos1);
        } else {
            done = true;
        }

        position += pos1 + 1;
    }
}

void util::split_string(const std::string& temp_string, const std::string& split_string,
                        std::vector<uint64_t>& split_positions) {
    bool done = false;

    uint64_t position = 0;

    while (!done) {
        const size_t pos1 = temp_string.substr(position).find(split_string);

        if (pos1 != std::string::npos) {
            split_positions.push_back(position + pos1);
        } else {
            done = true;
        }

        position += pos1 + 1;
    }
}

void util::split_string(const std::string& temp_string, const std::string& split_string,
                        std::vector<std::string>& split_strings) {
    bool done = false;

    uint64_t position = 0;
    uint64_t last_position = 0;

    while (!done) {
        size_t pos1 = temp_string.substr(position).find(split_string);

        if (pos1 != std::string::npos) {
            split_strings.emplace_back(temp_string.substr(position, pos1));
            pos1 += split_string.length() - 1;
            last_position = position + pos1 + 1;
        }
        else {
            done = true;
        }

        position += pos1 + 1;
    }

    split_strings.emplace_back(temp_string.substr(last_position, temp_string.length()));
}

std::string util::string_to_hex_string(const std::string input_string) {
    std::string output_string;

    for (uint64_t k = 0; k < input_string.size(); k++) {
        char value[5];
        sprintf_s(value, "\\x%02X", static_cast<unsigned char>(input_string.data()[k]));
        output_string += value;
    }

    return output_string;
}

std::vector<std::string> util::parse_input_filter(std::string input_string) {
    std::vector<std::string> filters;

    std::smatch m;
    std::regex re("^([^, ]+)[, ]+");
    std::regex_search(input_string, m, re);

    if (m.size() > 0) {
        filters.push_back(util::to_upper_case(m[1].str()));

        std::smatch m2;
        re.assign("[, ]+([^, ]+)");

        while (std::regex_search(input_string, m2, re)) {
            filters.push_back(util::to_upper_case(m2[1].str()));

            input_string = m2.suffix().str();
        }
    } else {
        filters.push_back(util::to_upper_case(input_string));
    }

    return filters;
}

std::string util::to_upper_case(const std::string& s) {
    std::string output(s.length(), ' ');

    for (uint64_t i = 0; i < s.length(); i++)
        output[i] = std::toupper(s[i]);

    return output;
}

std::string util::to_lower_case(const std::string& s) {
    std::string output(s.length(), ' ');

    for (uint64_t i = 0; i < s.length(); i++)
        output[i] = std::tolower(s[i]);

    return output;
}

std::string util::remove_all_string_from_string(std::string input_string, const std::string& to_remove) {
    bool done = false;

    while (!done) {
        const size_t pos = input_string.find(to_remove);

        if (pos != std::string::npos) {
            input_string.erase(pos, to_remove.length());
        } else {
            done = true;
        }
    }

    return input_string;
}

std::string util::uint64_t_to_hex_string_for_qn(const uint64_t bytes8) {
    std::stringstream ss;
    ss << std::hex << bytes8;
    return ss.str();
}

std::string util::uint64_t_to_hex_string(const uint64_t bytes8) {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << bytes8;
    return ss.str();
}

std::string util::uint32_t_to_hex_string(const uint32_t bytes4) {
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << bytes4;
    return ss.str();
}

std::string util::uint16_t_to_hex_string(const uint16_t bytes2) {
    std::stringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << bytes2;
    return ss.str();
}

std::string util::uint8_t_to_hex_string(const uint8_t bytes1) {
    std::stringstream ss;
    ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase
       << static_cast<int>(static_cast<unsigned char>(bytes1));
    return ss.str();
}

std::string util::uint64_t_to_string(const uint64_t bytes8) {
    std::stringstream ss;
    ss << bytes8;
    return ss.str();
}

std::string util::uint32_t_to_string(const uint32_t bytes4) {
    std::stringstream ss;
    ss << bytes4;
    return ss.str();
}

std::string util::uint16_t_to_string(const uint16_t bytes2) {
    std::stringstream ss;
    ss << bytes2;
    return ss.str();
}

std::string util::short_to_string(const short bytes2) {
    std::stringstream ss;
    ss << bytes2;
    return ss.str();
}

std::string util::bool_to_string(const bool value) {
    if (value) {
        return "True";
    }
    return "False";
}

std::string util::uint8_t_to_string(const uint8_t bytes1) {
    std::stringstream ss;
    ss << static_cast<int>(static_cast<unsigned char>(bytes1));
    return ss.str();
}

std::string util::int32_t_to_string(const int32_t bytes4) {
    std::stringstream ss;
    ss << bytes4;
    return ss.str();
}

std::string util::float_to_string(const float bytes4) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(10) << bytes4;
    return ss.str();
}

void
util::replace_all_string_in_string(std::string& input, const std::string& to_replace, const std::string& replace_with) {
    std::string new_string;
    new_string.reserve(input.length());

    std::string::size_type last_pos = 0;
    std::string::size_type find_pos;

    while (std::string::npos != (find_pos = input.find(to_replace, last_pos))) {
        new_string.append(input, last_pos, find_pos - last_pos);
        new_string += replace_with;
        last_pos = find_pos + to_replace.length();
    }

    new_string += input.substr(last_pos);

    input.swap(new_string);
}

bool util::float_equal(const float float_existing, const float float_new, const float tolerance) {
    const float float_new_abs = std::abs(float_new);
    const float float_existing_abs = std::abs(float_existing);
    float difference;

    if (float_new_abs > float_existing_abs) {
        difference = float_new_abs * tolerance;
    } else {
        difference = float_existing_abs * tolerance;
    }

    if (std::abs(float_new - float_existing) > difference) {
        return false;
    }
    return true;
}

uint32_t util::uint32_t_byteswap(uint32_t input) {
#if _MSC_VER
    return _byteswap_ulong(input);
#else
    return ((input & 0x000000FF) << 24) |
            ((input & 0x0000FF00) << 8) |
            ((input & 0x00FF0000) >> 8) |
            ((input & 0xFF000000) >> 24);
#endif
}