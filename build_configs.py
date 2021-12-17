release = ["-DCMAKE_BUILD_TYPE=Release"]
debug = ["-DCMAKE_BUILD_TYPE=Debug"]
# USE_GLIBCXX_DEBUG is not compatible with USE_LP (see issue983).
glibcxx_debug = ["-DCMAKE_BUILD_TYPE=Debug", "-DUSE_LP=NO", "-DUSE_GLIBCXX_DEBUG=YES"]
minimal = ["-DCMAKE_BUILD_TYPE=Release", "-DDISABLE_PLUGINS_BY_DEFAULT=YES"]
testing = minimal + [ "-DPLUGIN_{0}_ENABLED=YES".format(plugin) for plugin in [
            "POLICYFUZZING",
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
            ]]

DEFAULT = "release"
DEBUG = "debug"
