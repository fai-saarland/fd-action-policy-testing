release = ["-DCMAKE_BUILD_TYPE=Release"]
debug = ["-DCMAKE_BUILD_TYPE=Debug"]
# USE_GLIBCXX_DEBUG is not compatible with USE_LP (see issue983).
glibcxx_debug = ["-DCMAKE_BUILD_TYPE=Debug", "-DUSE_LP=NO", "-DUSE_GLIBCXX_DEBUG=YES"]
minimal = ["-DCMAKE_BUILD_TYPE=Release", "-DDISABLE_PLUGINS_BY_DEFAULT=YES"]
testing = minimal + [ "-DPLUGIN_POLICYFUZZING_ENABLED=YES", "-DPLUGIN_MAX_HEURISTIC_ENABLED=YES" ]

DEFAULT = "release"
DEBUG = "debug"
