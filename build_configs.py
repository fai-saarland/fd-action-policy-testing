_testing = [ "-DDISABLE_PLUGINS_BY_DEFAULT=YES" ] + [ "-DPLUGIN_{0}_ENABLED=YES".format(plugin) for plugin in [
            "POLICY_TESTING",
            "MAX_HEURISTIC",
            "FF_HEURISTIC",
            "LANDMARK_CUT_HEURISTIC",
            "ITERATED_SEARCH",
            "PLUGIN_LAZY_WASTAR",
            "PLUGIN_LAZY_GREEDY",
            "PLUGIN_EAGER_WASTAR",
            "PLUGIN_EAGER_GREEDY",
            "PLUGIN_ASTAR",
            "LANDMARKS",
            # "MAS_HEURISTIC",
            "ENFORCED_HILL_CLIMBING_SEARCH",
            "BLIND_SEARCH_HEURISTIC",
            ]]

# testing = ["-DCMAKE_BUILD_TYPE=Release"] + _testing
release = ["-DCMAKE_BUILD_TYPE=Release"] + _testing
release_custom_boost = ["-DCMAKE_BUILD_TYPE=Release", "-DBoost_NO_SYSTEM_PATHS=TRUE", "-DBOOST_ROOT=/mnt/data_server/eisenhut/opt"] + _testing
debug = ["-DCMAKE_BUILD_TYPE=Debug"] + _testing
release_clang = ["-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=/usr/bin/clang++"] + _testing
debug_clang = ["-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_CXX_COMPILER=/usr/bin/clang++"] + _testing
release_gcc = ["-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=/usr/bin/g++"] + _testing
debug_gcc = ["-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_CXX_COMPILER=/usr/bin/g++"] + _testing
# USE_GLIBCXX_DEBUG is not compatible with USE_LP (see issue983).
glibcxx_debug = ["-DCMAKE_BUILD_TYPE=Debug", "-DUSE_LP=NO", "-DUSE_GLIBCXX_DEBUG=YES"] + _testing
minimal = ["-DCMAKE_BUILD_TYPE=Release", "-DDISABLE_PLUGINS_BY_DEFAULT=YES"]

DEFAULT = "release"
DEBUG = "debug"
