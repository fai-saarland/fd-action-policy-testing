testing_options = [ "-DDISABLE_LIBRARIES_BY_DEFAULT=YES", "-DLIBRARY_POLICY_TESTING_ENABLED=YES"]

release = ["-DCMAKE_BUILD_TYPE=Release"] + testing_options
debug = ["-DCMAKE_BUILD_TYPE=Debug"] + testing_options
minimal = ["-DCMAKE_BUILD_TYPE=Release", "-DDISABLE_LIBRARIES_BY_DEFAULT=YES"]

DEFAULT = "release"
DEBUG = "debug"
