#include "config.h"

namespace yuan {

/**
 * @brief 递归列出所有node，相当于把node结构拍平。即DFS多叉树结构
 */
static void ListAllMember(const std::string &prefix,
                          const YAML::Node &node,
                          std::list<std::pair<std::string, YAML::Node>> &output) {
    if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
        YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "Config invalid name: " << prefix << " : " << node;
        return;
    }
    output.push_back({prefix, node});
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            ListAllMember(prefix.empty() ? it->first.Scalar() : (prefix + "." + it->first.Scalar()), it->second, output);
        }
    }
}

/**
 * @brief 以下是Config的方法定义 
 */
ConfigVarBase::ptr Config::LookupBase(const std::string &name) {
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}

// 根据yaml文件（配置）中的值，相应修改已经存储在Config中（约定）的值
void Config::LoadFromYaml(const YAML::Node &root) {
    std::list<std::pair<std::string, YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);

    for (auto &i : all_nodes) {
        std::string key = i.first;
        if (key.empty()) {
            continue;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);

        if (var) {
            if (i.second.IsScalar()) {
                var->fromString(i.second.Scalar());
            } else {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}
}