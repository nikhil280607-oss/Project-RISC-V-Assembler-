#include <bits/stdc++.h> 

using namespace std;

const unsigned int text_start_addr = 0x0;  //code segment starts here
const unsigned int data_start_addr = 0x10000000;   //data segment starts here
const unsigned int instruction_size = 4; //32 bits

struct instr_info {
    string fmt;
    string opcode;
    string funct3;
    string funct7;
};

map<string, string> register_map;

map<string, instr_info> instructions = {
    // R-format 
    {"add",   {"R", "0110011", "000", "0000000"}}, {"and",   {"R", "0110011", "111", "0000000"}},
    {"or",    {"R", "0110011", "110", "0000000"}}, {"sll",   {"R", "0110011", "001", "0000000"}},
    {"slt",   {"R", "0110011", "010", "0000000"}}, {"sra",   {"R", "0110011", "101", "0100000"}},
    {"srl",   {"R", "0110011", "101", "0000000"}}, {"sub",   {"R", "0110011", "000", "0100000"}},
    {"xor",   {"R", "0110011", "100", "0000000"}},
    {"mul",   {"R", "0110011", "000", "0000001"}}, {"div",   {"R", "0110011", "100", "0000001"}},
    {"rem",   {"R", "0110011", "110", "0000001"}}, 
    {"addw",  {"R", "0111011", "000", "0000000"}}, {"subw",  {"R", "0111011", "000", "0100000"}},
    {"mulw",  {"R", "0111011", "000", "0000001"}}, {"divw",  {"R", "0111011", "100", "0000001"}},
    {"remw",  {"R", "0111011", "110", "0000001"}},
    // I-format
    {"addi",  {"I", "0010011", "000", ""}}, {"addiw", {"I", "0011011", "000", ""}},
    {"andi",  {"I", "0010011", "111", ""}}, {"ori",   {"I", "0010011", "110", ""}},
    {"lb",    {"I", "0000011", "000", ""}}, {"ld",    {"I", "0000011", "011", ""}},
    {"lh",    {"I", "0000011", "001", ""}}, {"lw",    {"I", "0000011", "010", ""}},
    {"jalr",  {"I", "1100111", "000", ""}},
    // S-format
    {"sb",    {"S", "0100011", "000", ""}}, {"sw",    {"S", "0100011", "010", ""}},
    {"sh",    {"S", "0100011", "001", ""}}, {"sd",    {"S", "0100011", "011", ""}},
    // SB-format
    {"beq",   {"SB", "1100011", "000", ""}}, {"bne",   {"SB", "1100011", "001", ""}},
    {"blt",   {"SB", "1100011", "100", ""}}, {"bge",   {"SB", "1100011", "101", ""}},
    // U-format
    {"auipc", {"U", "0010111", "", ""}}, {"lui",   {"U", "0110111", "", ""}},
    // UJ-format
    {"jal",   {"UJ", "1101111", "", ""}}
};



struct parsed_instruction {
    unsigned int addr;
    string raw_line;
    string instruction;
    vector<string> args;
    string fmt;
};

struct data_directive {
    unsigned int addr;
    string raw_line;
    size_t size;
    string directive;
    string operands;
};

string clean_line(const string& line) {
    size_t comment_pos = line.find('#');
    string cleaned = line.substr(0, comment_pos);

    cleaned.erase(cleaned.begin(), find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) {
        return !isspace(ch);
    }));
    cleaned.erase(find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) {
        return !isspace(ch);
    }).base(), cleaned.end());

    return cleaned;
}

string int_to_binary_5bit(int n) {
    string bin = "";
    for (int i = 4; i >= 0; --i) {
        bin += ((n >> i) & 1) ? '1' : '0';
    }
    return bin;
}


void initialize_registers() {
    for (int i = 0; i < 32; ++i) {
        register_map["x" + to_string(i)] = int_to_binary_5bit(i);
    }
    register_map["zero"] = register_map["x0"];
    register_map["ra"] = register_map["x1"];
    register_map["sp"] = register_map["x2"];
    register_map["a0"] = register_map["x10"];
    register_map["a1"] = register_map["x11"];
    register_map["t0"] = register_map["x5"];
    register_map["t1"] = register_map["x6"];
    register_map["t2"] = register_map["x7"];
    register_map["s0"] = register_map["x8"];
    register_map["s1"] = register_map["x9"];
}

string to_twos_complement(long long value, int bits) {
    long long abs_value = abs(value);
    string abs_bin = "";
    
    if (abs_value == 0) {
        abs_bin = "0";
    } else {
        while (abs_value > 0) {
            abs_bin += ((abs_value % 2) == 0 ? '0' : '1');
            abs_value /= 2;
        }
    }

    while (abs_bin.length() < (size_t)bits) {
        abs_bin += '0';
    }
    reverse(abs_bin.begin(), abs_bin.end());
    abs_bin = abs_bin.substr(abs_bin.length() - bits);
    
    if (value < 0) {
        string twos_comp = "";
        bool carry = true; 
        for (int i = bits - 1; i >= 0; --i) {
            char bit = abs_bin[i];
            char flipped_bit = (bit == '0' ? '1' : '0');
            
            if (carry) {
                if (flipped_bit == '1') {
                    twos_comp += '0';
                    carry = true;
                } else {
                    twos_comp += '1';
                    carry = false;
                }
            } else {
                twos_comp += flipped_bit;
            }
        }
        reverse(twos_comp.begin(), twos_comp.end());
        return twos_comp;
    }
    
    return abs_bin;
}


unsigned int align_address(unsigned int addr, size_t alignment_size) {
    if (alignment_size == 0 || (addr % alignment_size) == 0) {
        return addr;
    }
    return addr + (alignment_size - (addr % alignment_size));
}


class assembler {
private:
    map<string, unsigned int> symbol_table;
    vector<parsed_instruction> text_instructions;
    vector<data_directive> data_directives;

    long long parse_long(const string& val_str) {
        try {
            if (val_str.empty()) return 0;
            
            if (val_str.size() > 2 && val_str[0] == '0' && (val_str[1] == 'x' || val_str[1] == 'X')) {
                return stoll(val_str.substr(2), nullptr, 16);
            }
            return stoll(val_str);
        } catch (const exception& e) {
            throw runtime_error("Invalid integer literal: " + val_str);
        }
    }
    
    vector<long long> get_data_operands(const string& operands_str) {
        vector<long long> values;
        stringstream ss(operands_str);
        string item_str;
        
        while (getline(ss, item_str, ',')) {
            string cleaned_item = clean_line(item_str);
            if (!cleaned_item.empty()) {
                values.push_back(parse_long(cleaned_item));
            }
        }
        return values;
    }


    bool parse_line(const string& line, string& instruction, vector<string>& args, string& format) {
        stringstream ss(line);
        ss >> instruction;
        
        transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

        if (instructions.find(instruction) == instructions.end()) {
            return false;
        }
        format = instructions[instruction].fmt;

        string arg_str;
        getline(ss, arg_str);

        replace(arg_str.begin(), arg_str.end(), ',', ' ');
        replace(arg_str.begin(), arg_str.end(), '(', ' ');
        replace(arg_str.begin(), arg_str.end(), ')', ' ');

        stringstream arg_ss(arg_str);
        string arg;
        while (arg_ss >> arg) {
            args.push_back(arg);
        }
        return true;
    }

    pair<string, string> encode_imm_s(long long imm) {
        string imm12 = to_twos_complement(imm, 12);
        return {imm12.substr(0, 7), imm12.substr(7, 5)};
    }
    
    pair<string, string> encode_imm_sb(long long imm) {
        long long offset = imm / 2; 
        string imm12 = to_twos_complement(offset, 12); 
        
        string imm_12   = imm12.substr(0, 1);      
        string imm_11   = imm12.substr(1, 1);      
        string imm_10_5 = imm12.substr(2, 6);      
        string imm_4_1  = imm12.substr(8, 4);      

        string imm_part1 = imm_12 + imm_10_5; 
        string imm_part2 = imm_4_1 + imm_11;  
        
        return {imm_part1, imm_part2};
    }
    
    string encode_imm_i(long long imm) { return to_twos_complement(imm, 12); }
    
    string encode_imm_u(long long imm) { 
        return bitset<20>(imm).to_string(); 
    }
    
    string encode_imm_uj(long long imm) {
        long long offset = imm / 2;
        string imm20 = to_twos_complement(offset, 20);

        string imm_20   = imm20.substr(0, 1);     
        string imm_10_1 = imm20.substr(10, 10);   
        string imm_11   = imm20.substr(9, 1);     
        string imm_19_12 = imm20.substr(1, 8);    

        return imm_20 + imm_10_1 + imm_11 + imm_19_12;
    }



public:
    void pass_one(const vector<string>& lines) {
        string current_segment = ".text";
        unsigned int current_addr = text_start_addr;

        for (size_t i = 0; i < lines.size(); ++i) {
            string cleaned = clean_line(lines[i]);
            if (cleaned.empty()) continue;

            if (cleaned.back() == ':') {
                string label = cleaned.substr(0, cleaned.length() - 1);
                
                if (symbol_table.count(label)) {
                    throw runtime_error("Duplicate label definition: " + label);
                }
                

                symbol_table[label] = current_addr;
                continue; 
            }

            if (cleaned.rfind('.', 0) == 0) { 
                
                stringstream ss(cleaned);
                string directive;
                ss >> directive;
                transform(directive.begin(), directive.end(), directive.begin(), ::tolower);

                if (directive == ".text") {
                    current_segment = ".text";
                    current_addr = text_start_addr;
                } else if (directive == ".data") {
                    current_segment = ".data";
                    current_addr = data_start_addr;
                } else if (current_segment == ".data") {
                    string operands_str;
                    getline(ss, operands_str);
                    operands_str = clean_line(operands_str);

                    size_t item_size = 0;
                    if (directive == ".byte") item_size = 1;
                    else if (directive == ".half") item_size = 2;
                    else if (directive == ".word") item_size = 4;
                    else if (directive == ".dword") item_size = 8;
                    else if (directive == ".asciz" || directive == ".asciiz") {
                      
                    } else {
                        throw runtime_error("Unknown data directive: " + directive);
                    }
                    
            
                    if (item_size > 1) {
                         current_addr = align_address(current_addr, item_size);
                    }
                    
                    data_directive dd;
                    dd.addr = current_addr;
                    dd.raw_line = cleaned;
                    dd.directive = directive;
                    dd.operands = operands_str;
                    dd.size = 0;

                    if (directive == ".asciz" || directive == ".asciiz") {
                        size_t first_quote = operands_str.find('"');
                        size_t last_quote = operands_str.rfind('"');
                        if (first_quote != string::npos && last_quote != string::npos && first_quote != last_quote) {
                            dd.size = (last_quote - first_quote - 1) + 1;
                        } else {
                            throw runtime_error("Invalid " + directive + " format: " + cleaned);
                        }
                    } else {
                        size_t count = get_data_operands(operands_str).size();
                        dd.size = count * item_size;
                    }
                    
                    if (dd.size > 0) {
                        data_directives.push_back(dd);
                        current_addr += dd.size;
                    }
                }
                continue;
            }

      
            if (current_segment == ".text") {
                string instruction, fmt;
                vector<string> args;
                if (parse_line(cleaned, instruction, args, fmt)) {
                    
                    current_addr = align_address(current_addr, instruction_size); 
                    
                    parsed_instruction pi;
                    pi.addr = current_addr;
                    pi.raw_line = cleaned;
                    pi.instruction = instruction;
                    pi.args = args;
                    pi.fmt = fmt;
                    text_instructions.push_back(pi);
                    current_addr += instruction_size;
                } else {
                    throw runtime_error("Invalid instruction or format: " + cleaned);
                }
            }
        }
    }
    

    vector<string> pass_two() {
        vector<string> mc_output;

        for (const auto& pi : text_instructions) {
            const instr_info& info = instructions[pi.instruction];
            string mc_bin = "";
            string metadata = "";
            unsigned int mc_hex_val = 0;

            string rd = "", rs1 = "", rs2 = "", imm = "";

            if (pi.fmt == "R") {
                rd = register_map[pi.args[0]]; rs1 = register_map[pi.args[1]]; rs2 = register_map[pi.args[2]];
                mc_bin = info.funct7 + rs2 + rs1 + info.funct3 + rd + info.opcode;
                metadata = info.opcode + "-" + info.funct3 + "-" + info.funct7 + "-" + rd + "-" + rs1 + "-" + rs2 + "-NULL";
            
            } else if (pi.fmt == "I") {
                rd = register_map[pi.args[0]]; rs1 = register_map[pi.args[1]];
                long long imm_val = parse_long(pi.args[2]);
                imm = encode_imm_i(imm_val);
                mc_bin = imm + rs1 + info.funct3 + rd + info.opcode;
                metadata = info.opcode + "-" + info.funct3 + "-NULL-" + rd + "-" + rs1 + "-NULL-" + imm;

            } else if (pi.fmt == "S") {
                rs2 = register_map[pi.args[0]]; rs1 = register_map[pi.args[1]];
                long long imm_val = parse_long(pi.args[2]);

                pair<string, string> imm_parts = encode_imm_s(imm_val);
                string imm7 = imm_parts.first; 
                string imm5 = imm_parts.second; 

                mc_bin = imm7 + rs2 + rs1 + info.funct3 + imm5 + info.opcode;
                metadata = info.opcode + "-" + info.funct3 + "-" + imm7 + "-NULL-" + rs1 + "-" + rs2 + "-" + imm5;

            } else if (pi.fmt == "SB") {
                rs1 = register_map[pi.args[0]]; rs2 = register_map[pi.args[1]];
                string label = pi.args[2];

                if (!symbol_table.count(label)) 
                    throw runtime_error("Undefined label: " + label);

                long long target_addr = symbol_table[label];
                long long imm_val = target_addr - pi.addr;

                pair<string, string> imm_parts = encode_imm_sb(imm_val);
                string imm7 = imm_parts.first;  
                string imm5 = imm_parts.second; 

                mc_bin = imm7 + rs2 + rs1 + info.funct3 + imm5 + info.opcode;
                metadata = info.opcode + "-" + info.funct3 + "-" + imm7 + "-NULL-" + rs1 + "-" + rs2 + "-" + imm5;

            } else if (pi.fmt == "U") {
                rd = register_map[pi.args[0]];
                long long imm_val = parse_long(pi.args[1]);
                imm = encode_imm_u(imm_val); 
                mc_bin = imm + rd + info.opcode;
                metadata = info.opcode + "-NULL-NULL-" + rd + "-NULL-" + imm;

            } else if (pi.fmt == "UJ") {
                rd = register_map[pi.args[0]];
                string label = pi.args[1];

                if (!symbol_table.count(label)) 
                    throw runtime_error("Undefined label: " + label);

                long long target_addr = symbol_table[label];
                long long imm_val = target_addr - pi.addr;

                imm = encode_imm_uj(imm_val);
                mc_bin = imm + rd + info.opcode;
                metadata = info.opcode + "-NULL-NULL-" + rd + "-NULL-" + imm;
            
            } else {
                throw runtime_error("Unsupported format: " + pi.fmt);
            }

            mc_hex_val = stoul(mc_bin, nullptr, 2);

            stringstream ss;
            ss << "0x" << hex << uppercase << pi.addr << " "
               << "0x" << setw(8) << setfill('0') << mc_hex_val << " , "
               << pi.raw_line << " # " << metadata;
            mc_output.push_back(ss.str());
        }

        mc_output.push_back(""); 
        unsigned int end_text_addr = text_start_addr + (text_instructions.size() * instruction_size);
        stringstream ss_end;
        ss_end << "0x" << hex << uppercase << end_text_addr << " 0x00000000";
        mc_output.push_back(ss_end.str());
        mc_output.push_back(""); 

        for (const auto& dd : data_directives) {
            string directive = dd.directive;
            string operands = dd.operands;
            
            if (directive == ".asciz" || directive == ".asciiz") {
                
                size_t first_quote = operands.find('"');
                size_t last_quote = operands.rfind('"');
                string str_val = operands.substr(first_quote + 1, last_quote - first_quote - 1);
                
                stringstream ss_data;
                ss_data << "0x" << hex << uppercase << dd.addr << " 0x";
                
                for (size_t i = 0; i < str_val.length(); ++i) {
                     ss_data << setw(2) << setfill('0') << hex << (int)(unsigned char)str_val[i];
                }
                ss_data << "00";
                
                mc_output.push_back(ss_data.str());
                continue;
            } 
            
            
            vector<long long> values = get_data_operands(operands);
            size_t item_size = 0;
            if (directive == ".byte") item_size = 1;
            else if (directive == ".half") item_size = 2;
            else if (directive == ".word") item_size = 4;
            else if (directive == ".dword") item_size = 8;

            stringstream hex_data_stream;
            for (size_t i = 0; i < values.size(); ++i) {
                long long val = values[i];

                if (directive == ".byte") hex_data_stream << setw(2) << setfill('0') << hex << (val & 0xFF);
                else if (directive == ".half") hex_data_stream << setw(4) << setfill('0') << hex << (val & 0xFFFF);
                else if (directive == ".word") hex_data_stream << setw(8) << setfill('0') << hex << (val & 0xFFFFFFFF);
                else if (directive == ".dword") hex_data_stream << setw(16) << setfill('0') << hex << (val & 0xFFFFFFFFFFFFFFFFLL);
            }
            
            stringstream ss_item;
            ss_item << "0x" << hex << uppercase << dd.addr << " 0x" << hex_data_stream.str();
            
            mc_output.push_back(ss_item.str());
        }
        mc_output.push_back(""); 
        
        return mc_output;
    }
};


void run_assembler(const string& input_file, const string& output_file) {
    cout << "Starting RISC-V Assembler (C++ Two-Pass)..." << endl;
    ifstream input_fs(input_file);
    if (!input_fs.is_open()) {
        cerr << "Error: Cannot open input file '" << input_file << "'" << endl;
        return;
    }

    vector<string> lines;
    string line;
    while (getline(input_fs, line)) {
        lines.push_back(line);
    }
    input_fs.close();

    initialize_registers();
    
    assembler asm_instance;
    try {
        cout << "Pass 1: Calculating addresses and building symbol table " << endl;
        asm_instance.pass_one(lines);
        

        cout << "Pass 2: Generating Machine Code " << endl;
        vector<string> mc_lines = asm_instance.pass_two();

        ofstream output_fs(output_file);
        if (!output_fs.is_open()) {
            cerr << "Error: Cannot open output file '" << output_file << "'" << endl;
            return;
        }

        for (const string& l : mc_lines) {
            if (!l.empty()) { 
                output_fs << l << endl;
            } else {
                output_fs << endl; 
            }
        }
        output_fs.close();

        cout << "\n Assembly successful! Output written to '" << output_file << "'" << endl;

    } catch (const exception& e) {
        cerr << "\n assembly code error: " << e.what() << endl;
    }
}

int main(int argc, char* argv[]) {
    string input_file = "input.asm";
    string output_file = "output.mc";

    if (argc > 1) input_file = argv[1];
    if (argc > 2) output_file = argv[2];

    run_assembler(input_file, output_file);
    return 0;
}