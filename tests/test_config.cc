#include "../yuan/config.h"
#include "../yuan/log.h"
#include <yaml-cpp/yaml.h>

// 通过Config获得基础的配置项
yuan::ConfigVar<int>::ptr g_int_value_config =
    yuan::Config::Lookup("system.port", (int)8080, "system.port");

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/home/yuan/workspace/yuan/bin/conf/log.yml");

    YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << root;
}

int main() {
    YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << g_int_value_config->getValue();
    YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << g_int_value_config->toString();

    test_yaml();

    return 0;
}