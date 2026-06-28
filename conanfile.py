from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, CMake, cmake_layout
from conan.tools.files import copy

class Radapter(ConanFile):
    name = "radapter"
    version = "3.0"
    url = "https://github.com/cyanidle/radapter"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "gui": [True, False],
        "shared": [True, False],
        "fPIC": [True, False],
        "lib_only": [True, False],
        "jit": ["off", "static", "shared"],
    }

    default_options = {
        "gui": True,
        "shared": True,
        "fPIC": True,
        "lib_only": False,
        "jit": "shared",
    }

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("openssl/[~3]")
        # TODO: add qtmqtt, qtgrpc, qtopcua
        # TODO: https://mirror.yandex.ru/mirrors/qt.io/official_releases/qt/
        qtopts = {
            "qtserialbus": True,
            "qtwebsockets": True,
            "qtdeclarative": True,
            "qtserialport": True
        }
        if self.options.gui:
            qtopts.update({
                "qtserialbus": True,
                "qtwebsockets": True,
                "qtdeclarative": True,
                "qtserialport": True,
                "with_harfbuzz": True,
                "qtshadertools": True,
                "qttranslations": True,
                "qttools": True,
                "qtcharts": True,
                "qtgraphs": True,
                "with_gssapi": True,
            })
            if self.settings.os == "Linux":
                qtopts.update({
                    "with_dbus": True,
                })
        self.requires("qt/[~6]", options=qtopts)
        if self.options.jit != "off":
            self.requires("luajit/2.1.0-beta3", options={"shared": self.options.jit == "shared"})

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["RADAPTER_JIT"] = self.options.jit != "off"
        tc.variables["RADAPTER_JIT_STATIC"] = self.options.jit != "shared"
        tc.variables["RADAPTER_GUI"] = bool(self.options.gui)
        tc.variables["RADAPTER_STATIC"] = not self.options.shared
        tc.generate()

        copy(self, "plugins/*", self.dependencies["qt"].package_folder, self.build_folder)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

        copy(self, "plugins/*", self.build_folder, self.package_folder)

    def package_info(self):
        self.cpp_info.libs += ["radapter-sdk"]