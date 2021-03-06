///<------------------------------------------------------------------------------
//< @file:   prop\parser\gen_cpp.cpp
//< @author: 洪坤安
//< @date:   2014年9月25日 23:13:52
//< @brief:  
//< Copyright (c) 2014 Tokit. All rights reserved.
///<------------------------------------------------------------------------------

#include "c++_generator.h"

#include "file_tool.h"
#include "str_tool.h"
#include "echo_tool.h"
#include "cfg.h"
#include "parser.h"

#include <fstream>

using namespace filetool;
using namespace std;

namespace cxx
{
    // 生成结构体的构造函数
    string gen_struct_ctor(const cfg_t &cfg)
    {
        string ctor_text = 
            "%cfg%::%cfg%()"
            ;       

        strtool::replace(ctor_text, "%cfg%", cpptool::get_cfg_type_name(cfg));

        for(size_t n = 0; n < cfg.fields.size(); ++n){
            const field_t &field = cfg.fields[n];
            if (field.is_array() || field.is_set()){
                continue;
            }

            ctor_text += n ? "\n    , " :
                             "\n    : ";
            ctor_text += (field.fieldtype == fieldtype_string) ? field.en_name + "(\"\")" :
                                                                 field.en_name + "(0)";
        }

        ctor_text += "\n{\n}";
        return ctor_text;
    }

    // 生成结构体的构造函数
    string gen_struct_print(const cfg_t &cfg)
    {
        string print_text = 
            "std::string %cfg%::print() const"
            "\n{"
            "%print%"
            "\n}"
            ;       

        strtool::replace(print_text, "%cfg%", cpptool::get_cfg_type_name(cfg));

        string printf_fmt;
        string printf_argv;

        static const string static_printf_format[fieldtype_max] = {
            "",
            "%s",
            "%u",
            "%d",
            "%d",
            "%d",
            "\"I64d\"",
            "%u",
            "%u",
            "%u",
            "\"I64u\"",
            "%f",
            "%lf",
        };

        size_t n_fields = cfg.fields.size();
        for(size_t n = 0; n < n_fields; ++n){
            const field_t &field = cfg.fields[n];
            if (field.is_array() || field.is_set()){
                continue;
            }

            printf_fmt  += field.en_name + "=" + static_printf_format[field.fieldtype];
            if (n + 1 < n_fields){
                printf_fmt += " ";
            }

            if (0 == n % 10 && n > 1){
                printf_fmt += "\"\n        \"";
            }

            printf_argv += "\n        , ";
            printf_argv += (field.fieldtype == fieldtype_string) ? field.en_name + ".c_str()" :
                                                                   field.en_name;
        }

        string print_stmt;

        if(n_fields > 0){
            print_stmt = "\n    char buf[2048];"
                         "\n    snprintf(buf, sizeof(buf), \"%printf_fmt%\"%printf_argv%);"
                         "\n"
                         "\n    return buf;";

            strtool::replace(print_stmt, "%printf_fmt%", printf_fmt);
            strtool::replace(print_stmt, "%printf_argv%", printf_argv);
        }

        strtool::replace(print_text, "%print%", print_stmt);
        return print_text;
    }

	// 生成读取属性的语句
	string gen_load_attr_statement(const field_t &field)
	{
		static const string static_load_attr_stmt[fieldtype_max] = {
			"",
			"cfg.%field% = %cast%(%val%);",
            "cfg.%field% = (%val%[0] != '0');",
            "cfg.%field% = %cast%(%val%);",
            "cfg.%field% = %cast%(%val%);",
            "cfg.%field% = %cast%(%val%);",
            "cfg.%field% = %cast%(%val%);",
			"cfg.%field% = %cast%(%val%);",
			"cfg.%field% = %cast%(%val%);",
			"cfg.%field% = %cast%(%val%);",
			"cfg.%field% = %cast%(%val%);",
			"cfg.%field% = (float)%cast%(%val%);",
			"cfg.%field% = %cast%(%val%);",
		};

        static const string static_cast_func_name[fieldtype_max] = {
            "",
            "strtool::un_escape_xml",
            "strtool::str_to_bool",
            "strtool::str_to_int32",
            "strtool::str_to_int32",
            "strtool::str_to_int32",
            "strtool::str_to_int64",
            "strtool::str_to_uint32",
            "strtool::str_to_uint32",
            "strtool::str_to_uint32",
            "strtool::str_to_uint64",
            "atof",
            "atof",
        };

        static const string static_load_vec_stmt = "cfg.%field% = strtool::split_str_to_vec<%T%>(%val%, %cast%);";
        static const string static_load_set_stmt = "cfg.%field% = strtool::split_str_to_set<%T%>(%val%, %cast%);";

        static const string static_load_str_vec_stmt = "cfg.%field% = strtool::split(%val%);";
        static const string static_load_str_set_stmt = "cfg.%field% = strtool::split_str_set(%val%);";

		string stmt = static_load_attr_stmt[field.fieldtype];
        if (field.is_array()){
            if (field.fieldtype == fieldtype_string){
                stmt = static_load_str_vec_stmt;
            }else{
                stmt = static_load_vec_stmt;
            }
        }else if (field.is_set()){
            if (field.fieldtype == fieldtype_string){
                stmt = static_load_str_set_stmt;
            }else{
                stmt = static_load_set_stmt;
            }
        }

        string val = "node->first_attribute(\"%field%\")->value()";
        const string &cast_func = static_cast_func_name[field.fieldtype];
        strtool::replace(stmt, "%val%", val);
        strtool::replace(stmt, "%field%", field.en_name);
        strtool::replace(stmt, "%T%", cpp_generator::raw_type_2_c_type(field.fieldtype));
        strtool::replace(stmt, "%cast%", cast_func);
		return stmt;
	}

	// 拼接字符串键值
	// eg: string key = tostr(prop.type) + tostr(prop.id) + prop.name;
	// eg: string key = tostr(prop.type) + prop.name;
	// eg: string &key = prop.name;
	string splice_str_key(fieldvec_t keys)
	{
		string ret = "string key = ";
		if(keys.size() == 1){
			ret = "std::string &key = ";
		}else{
			ret = "std::string key = ";
		}

		for (fieldvec_t::iterator itr = keys.begin(); itr != keys.end(); ++itr){
			field_t &key = *itr;

			if(key.fieldtype == fieldtype_string){
				ret += key.en_name;
			}else{
				ret += "strtool::tostr(" + key.en_name + ")";
			}

			ret += " + ";
		}

		ret.erase(ret.end() - 3, ret.end());

		ret += ";";
		return ret;
	}

	// splice the key string
	// eg: string key = tostr(prop.type) + tostr(prop.id) + prop.name;
	// eg: string key = tostr(prop.type) + prop.name;
	// eg: string &key = prop.name;
	string splice_int_key(fieldvec_t &keys)
	{
		size_t n_key = keys.size();
		if(n_key <= 1){
			return "";
		}

		string ret = "";
		switch(n_key)
		{
		// 2个主键
		case 2:
			ret = "uint64 key = keytool::Get3232Key(";
			ret += keys[0].en_name;
			ret += ", ";
			ret += keys[1].en_name;
			ret += ");";
			break;

        // 3个主键
		case 3:
			ret = "uint64 key = keytool::Get161616Key(";
			ret += keys[0].en_name;
			ret += ", ";
			ret += keys[1].en_name;
			ret += ", ";
			ret += keys[2].en_name;
			ret += ");";
			break;

		// 4个主键
		case 4:
			ret = "uint64 key = keytool::Get16161616Key(";
			ret += keys[0].en_name;
			ret += ", ";
			ret += keys[1].en_name;
			ret += ", ";
			ret += keys[2].en_name;
			ret += ", ";
			ret += keys[3].en_name;
			ret += ");";

			break;
		}

		return ret;
	}

    string splice_n_key(const cfg_t &cfg, const char* prefix = "")
    {
        size_t n_key = cfg.keys.size();
        if(n_key <= 1){
            return "";
        }

        bool is_key_has_str = false;

        for (size_t n = 0; n < n_key; n++){
            int keyidx = cfg.keys[n];
            const field_t &key = cfg.fields[keyidx];
            if(key.fieldtype == fieldtype_string){
                is_key_has_str = true;
                break;
            }
        }

        fieldvec_t keys;
        for (size_t n = 0; n < n_key; n++){
            int keyidx = cfg.keys[n];
            field_t key = cfg.fields[keyidx]; // 注意，这里进行了赋值拷贝

            key.en_name = prefix + key.en_name;
            keys.push_back(key);
        }

        string text = is_key_has_str ? splice_str_key(keys) :
                                       splice_int_key(keys);
        
        return text;
    }

	string gen_n_key_map_insert_stmt(const cfg_t &cfg)
	{
		size_t n_key = cfg.keys.size();
		if(n_key <= 1){
			return "";
		}

		bool is_key_has_str = false;

        for (size_t n = 0; n < n_key; n++){
			int keyidx = cfg.keys[n];
			const field_t &key = cfg.fields[keyidx];

			if(key.fieldtype == fieldtype_string){
				is_key_has_str = true;
				break;
			}
		}

		fieldvec_t keys;
        for (size_t n = 0; n < n_key; n++){
            int keyidx = cfg.keys[n];
            field_t key = cfg.fields[keyidx]; // 注意，这里进行了赋值拷贝

			key.en_name = "cfg." + key.en_name;
			keys.push_back(key);
		}

		string map_key = "\n        " + splice_n_key(cfg, "cfg.");
		string map_insert = "\n        m_%map%[key] = curcfg;\n";
		strtool::replace(map_insert, "%map%", cpptool::get_n_key_map_name(cfg));

		return map_key + map_insert;
	}

    string gen_1_key_map_insert_stmt(const cfg_t &cfg)
    {
        string text = "";

        for(size_t n = 0; n < cfg.fields.size(); n++){
            const field_t &field = cfg.fields[n];
            if (!field.is_1_key() || field.is_n_key()){
                continue;
            }

            string insert_stmt = "\n        m_%map%[cfg.%key%] = curcfg;";

            strtool::replace(insert_stmt, "%map%", cpptool::get_1_key_map_name(cfg, field));
            strtool::replace(insert_stmt, "%key%", field.en_name);
            text += insert_stmt;
        }

        strtool::replace(text, "%cfg%", cfg.en_name);
        return text;
    }

    string gen_load_func(const cfg_t &cfg)
    {
        string text = 
            "\n// %comment%"
            "\n%load_func_name%"
            "\n{"
            "\n    tick_t load_tick;"
            "\n"
            "\n    rapidxml::file<> fdoc(this->get_path(\"%cfg%.xml\").c_str());"
            "\n    rapidxml::xml_document<> doc;"
            "\n    if(!fdoc.data()){"
            "\n        std::cout << \"err: load \" << this->get_path(\"%cfg%.xml\") << \" failed, please check the file path\" << std::endl;"
            "\n        return false;"
            "\n    }"
            "\n"
            "\n    doc.parse<0>(fdoc.data());"
            "\n"
            "\n    rapidxml::xml_node<>* root = doc.first_node();"
            "\n    if(!root){"
            "\n        std::cout << \"err: parse <%cfg%.xml> failed, please check xml file format\" << std::endl;"
            "\n        return false;"
            "\n    }"
            "\n"
            "\n    clear_%cfg%();"
            "\n"
            "\n    int n_row = 0;"
            "\n    for(rapidxml::xml_node<> *node = root->first_node(); node; node = node->next_sibling()){"
            "\n        ++n_row;"
            "\n    }"
            "\n%vec_reserve_stmt%"
            "\n    %cfgtype% cfg;"
            "\n    for(rapidxml::xml_node<> *node = root->first_node(); node; node = node->next_sibling()){"
            // "\n        rapidxml::xml_attribute<> *attr = node->first_attribute();"
            "\n"
            ;

        string vec_reserve_stmt;
        if (!cfg.only_has_1_row()){
            vec_reserve_stmt = "\n    m_%vec%.reserve(n_row);\n";
        }

        strtool::replace(text, "%vec_reserve_stmt%", vec_reserve_stmt);
        strtool::replace(text, "%cfg%", cfg.en_name);
        strtool::replace(text, "%load_func_name%", cpptool::get_load_func_declare(cfg));
        strtool::replace(text, "%comment%", cfg.cn_name);
        strtool::replace(text, "%vec%", cpptool::get_vec_name(cfg));
        strtool::replace(text, "%cfgtype%", cpptool::get_cfg_type_name(cfg));

        string load_attr_stmt;

        size_t n_field = cfg.fields.size();
        for(size_t n = 0; n < n_field; n++){
            const field_t &field = cfg.fields[n];

            load_attr_stmt = "        " + gen_load_attr_statement(field) + "\n";
            if (n * 2 + 1 > n_field){
                strtool::replace(load_attr_stmt, "node->first_attribute", "node->last_attribute");
            }

            text += load_attr_stmt;
        }

        if (!cfg.only_has_1_row()){
            string vec_push_stmt = 
                "\n        m_%vec%.push_back(cfg);"
                "\n        %cfgtype%* curcfg = &m_%vec%.back();"
                ;

            strtool::replace(vec_push_stmt, "%vec%", cpptool::get_vec_name(cfg));
            strtool::replace(vec_push_stmt, "%cfg%", cfg.en_name);
            strtool::replace(vec_push_stmt, "%cfgtype%", cpptool::get_cfg_type_name(cfg));

            string map_insert_stmt = "\n    ";
            map_insert_stmt += gen_n_key_map_insert_stmt(cfg);
            map_insert_stmt += gen_1_key_map_insert_stmt(cfg);

            text += vec_push_stmt;
            text += map_insert_stmt;
        }else{
            string eval_stmt = 
                "\n        m_%cfg% = cfg;"
                "\n        break;"
                ;

            strtool::replace(eval_stmt, "%cfg%", cfg.en_name);
            text += eval_stmt;
        }

        string end_stmt = 
            "\n    }"
            "\n"
            "\n    printf(\"load <%-20s> success, cost time = <%-6f>s\\n\", \"%cfg%.xml\", load_tick.end_tick());"
            "\n    return true;"
            "\n}"
            ;

        strtool::replace(end_stmt, "%cfg%", cfg.en_name);
        text += end_stmt;
        return text;
    }

    string gen_call_load_func_stmt(const cfg_t &cfg)
    {
        return "    is_succ &= load_" + cfg.en_name + "();";
    }

    string gen_call_clear_func_stmt(const cfg_t &cfg)
    {
        string text = "    " + cpptool::get_clear_func_name(cfg) + "();";
        return text;
    }

    string gen_clear_func(const cfg_t &cfg)
    {
        string text = 
            "\n%clear_func%"
            "\n{"
            "%clear_stmt%"
            "\n}"
            ;

        string clear_stmt;

        if (!cfg.only_has_1_row()){
            clear_stmt += "\n    m_" + cpptool::get_vec_name(cfg) + ".clear();";

            if (cfg.keys.size() > 1){
                clear_stmt += "\n    m_" + cpptool::get_n_key_map_name(cfg) + ".clear();";
            }

            size_t n_field = cfg.fields.size();

            for(size_t n = 0; n < n_field; n++){
                const field_t &field = cfg.fields[n];
                if (!field.is_1_key() || field.is_n_key()){
                    continue;
                }

                clear_stmt += "\n    m_" + cpptool::get_1_key_map_name(cfg, field) + ".clear();";
            }
        }else{
            clear_stmt = "\n    m_%cfg% = %cfgtype%();";
            strtool::replace(clear_stmt, "%cfgtype%", cpptool::get_cfg_type_name(cfg));
            strtool::replace(clear_stmt, "%cfg%", cfg.en_name);
        }

        strtool::replace(text, "%clear_func%", cpptool::get_clear_func_declare(cfg));
        strtool::replace(text, "%clear_stmt%", clear_stmt);
        return text;
    }

    string gen_n_key_find_func(const cfg_t &cfg)
    {
        const size_t n_key = cfg.keys.size();
        if(n_key <= 1){
            return "";
        }

        string text = 
            "\n%find_func%"
            "\n{"
            "\n    %key_stmt%"
            "\n    %map%::iterator itr = m_%map%.find(key);"
            "\n    if(itr == m_%map%.end()){"
            "\n        return NULL;"
            "\n    }"
            "\n"
            "\n    %cfg% *cfg = itr->second;"
            "\n    return cfg;"
            "\n}"
            "\n"
            ;

        strtool::replace(text, "%cfg%", cpptool::get_cfg_type_name(cfg));
        strtool::replace(text, "%find_func%", cpptool::get_n_key_find_func_declare(cfg));
        strtool::replace(text, "%key_stmt%", splice_n_key(cfg));
        strtool::replace(text, "%map%", cpptool::get_n_key_map_name(cfg));
        return text;
    }

    string gen_1_key_find_func(const cfg_t &cfg)
    {
        string templet = 
            "\n%find_func%"
            "\n{"
            "\n    %map%::iterator itr = m_%map%.find(%key%);"
            "\n    if(itr == m_%map%.end()){"
            "\n        return NULL;"
            "\n    }"
            "\n"
            "\n    %cfg% *cfg = itr->second;"
            "\n    return cfg;"
            "\n}"
            ;

        string text;
        for(size_t n = 0; n < cfg.fields.size(); n++){
            const field_t &field = cfg.fields[n];
            if (!field.is_1_key() || field.is_n_key()){
                continue;
            }

            string func_stmt = templet;

            strtool::replace(func_stmt, "%cfg%", cpptool::get_cfg_type_name(cfg));
            strtool::replace(func_stmt, "%find_func%", cpptool::get_1_key_find_func_declare(cfg, field));
            strtool::replace(func_stmt, "%map%", cpptool::get_1_key_map_name(cfg, field));
            strtool::replace(func_stmt, "%key%", field.en_name); 

            text += func_stmt + "\n";
        }

        strtool::replace(text, "%cfg%", cfg.en_name);
        return text;
    }

    string gen_find_func(const cfg_t &cfg)
    {
        if (cfg.only_has_1_row()){
            return "";
        }

        string text;
        text += cpptool::get_comment(cfg);
        text += gen_n_key_find_func(cfg) + gen_1_key_find_func(cfg);
        return text;
    }
}

bool cpp_generator::gen_cpp_file(const string &cpp_file)
{
    static string cpp_templet;
    if (cpp_templet.empty()){
        filetool::get_whole_file_str(m_cpp_templet_path, cpp_templet);

        if(cpp_templet.empty()){
            ECHO_ERR("生成c++文件失败：找不到模板文件<%s>", m_cpp_templet_path.c_str());
            return false;
        }
    }

    string src = cpp_templet;
    strtool::replace(src, "%cfg%", m_cfgbase.filename);

    strtool::replace(src, "%structs_ctor%", cpptool::splice(m_cfgbase, cxx::gen_struct_ctor, "\n\n"));
    strtool::replace(src, "%structs_print%", cpptool::splice(m_cfgbase, cxx::gen_struct_print, "\n\n"));
    strtool::replace(src, "%load_funcs%",   cpptool::splice(m_cfgbase, cxx::gen_load_func, "\n"));
    strtool::replace(src, "%clear_funcs%",  cpptool::splice(m_cfgbase, cxx::gen_clear_func, "\n"));
    strtool::replace(src, "%find_funcs%",   cpptool::splice(m_cfgbase, cxx::gen_find_func, "\n"));

    strtool::replace(src, "%mgr%",          cpptool::get_mgr_name(m_cfgbase));
    strtool::replace(src, "%load_stmt%",    cpptool::splice(m_cfgbase, cxx::gen_call_load_func_stmt, "\n"));
    strtool::replace(src, "%clear_stmt%",   cpptool::splice(m_cfgbase, cxx::gen_call_clear_func_stmt, "\n"));
    strtool::replace(src, "%cfg_member%",   cpptool::get_member_comment_list(m_cfgbase));

    bool is_overwrite_file_ok = filetool::overwrite_file(cpp_file, src);
    return is_overwrite_file_ok;
}
