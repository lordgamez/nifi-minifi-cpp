# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import os
import platform
import re
import inquirer

from minifi_option import MinifiOptions
from package_manager import PackageManager
from system_dependency import install_required


def get_rocksdb_version(source_dir) -> str:
    conanfile = source_dir / "conanfile.py"
    match = re.search(r"rocksdb/(?P<version>[^@\"']+)@minifi/develop", conanfile.read_text())
    if not match:
        raise ValueError("Could not find the custom rocksdb reference in conanfile.py")
    return match.group("version")


def install_dependencies(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    res = install_required(minifi_options, package_manager)
    print("Installation went smoothly" if res else "There were some error during installation")
    return res


def run_conan_install(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    if not minifi_options.use_conan.value == "ON":
        print("Conan install skipped because USE_CONAN is OFF")
        return True
    conan_options = ""
    if minifi_options.custom_malloc is not None and minifi_options.custom_malloc.value not in (None, "OFF"):
        conan_options = f'-o "&:custom_malloc={minifi_options.custom_malloc.value}"'
    if minifi_options.bool_options["ENABLE_SFTP"].value not in (None, "OFF"):
        conan_options += ' -o "&:enable_sftp=True"'
    if minifi_options.bool_options["ENABLE_PROMETHEUS"].value not in (None, "OFF"):
        conan_options += ' -o "&:enable_prometheus=True"'
    if minifi_options.bool_options["ENABLE_BZIP2"].value not in (None, "OFF"):
        conan_options += ' -o "&:enable_bzip2=True"'
    if minifi_options.bool_options["ENABLE_LZMA"].value not in (None, "OFF"):
        conan_options += ' -o "&:enable_lzma=True"'
    if minifi_options.bool_options["ENABLE_MQTT"].value not in (None, "OFF"):
        conan_options += ' -o "&:enable_mqtt=True"'
    if minifi_options.bool_options["SKIP_TESTS"].value not in (None, "OFF"):
        conan_options += ' -o "&:skip_tests=True"'
    if not package_manager.run_cmd("conan profile detect --exist-ok"):
        print("Conan default profile detection failed")
        return False

    rocksdb_recipe_dir = minifi_options.source_dir / "thirdparty" / "rocksdb" / "all"
    rocksdb_version = get_rocksdb_version(minifi_options.source_dir)
    if not package_manager.run_cmd(f"conan export {rocksdb_recipe_dir} --version={rocksdb_version} --user=minifi --channel=develop"):
        print("Exporting the custom RocksDB Conan recipe failed")
        return False

    compiler_settings = " --settings=compiler.cppstd=23"
    generator_setting = " -c tools.cmake.cmaketoolchain:generator=Ninja" if minifi_options.use_ninja.value == "ON" else ""
    build_cmd = f"conan install {minifi_options.source_dir} --output-folder={minifi_options.build_dir} --build=missing {conan_options} --settings=build_type={minifi_options.build_type.value}{generator_setting}{compiler_settings}"
    res = package_manager.run_cmd(build_cmd)
    print("Conan install was successful" if res else "Conan install was unsuccessful")
    return res


def _conan_build_env_prefix(minifi_options: MinifiOptions) -> str:
    if minifi_options.use_conan.value == "ON" and platform.system() == "Windows":
        conanbuild = os.path.join(str(minifi_options.build_dir), "conanbuild.bat")
        return f'call "{conanbuild}" && '
    return ""


def run_cmake(minifi_options: MinifiOptions, package_manager: PackageManager):
    if not os.path.exists(minifi_options.build_dir):
        os.mkdir(minifi_options.build_dir)
    cmake_cmd = f"{_conan_build_env_prefix(minifi_options)}cmake {minifi_options.create_cmake_generator_str()} {minifi_options.create_cmake_use_conan_str()} {minifi_options.create_cmake_options_str()} {minifi_options.source_dir} -B {minifi_options.build_dir}"
    res = package_manager.run_cmd(cmake_cmd)
    print("CMake command run successfully" if res else "CMake command run unsuccessfully")
    return res


def do_build(minifi_options: MinifiOptions, package_manager: PackageManager):
    build_cmd = f"{_conan_build_env_prefix(minifi_options)}cmake --build {str(minifi_options.build_dir)} {minifi_options.create_cmake_build_flags_str()}"
    res = package_manager.run_cmd(build_cmd)
    print("Build was successful" if res else "Build was unsuccessful")
    return res


def do_package(minifi_options: MinifiOptions, package_manager: PackageManager):
    build_cmd = f"{_conan_build_env_prefix(minifi_options)}cmake --build {str(minifi_options.build_dir)} --target package {minifi_options.create_cmake_build_flags_str()}"
    return package_manager.run_cmd(build_cmd)


def do_docker_build(minifi_options: MinifiOptions, package_manager: PackageManager):
    build_cmd = f"cmake --build {str(minifi_options.build_dir)} --target docker"
    return package_manager.run_cmd(build_cmd)


def do_one_click_build(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    assert install_dependencies(minifi_options, package_manager)
    assert run_conan_install(minifi_options, package_manager)
    assert run_cmake(minifi_options, package_manager)
    assert do_build(minifi_options, package_manager)
    assert do_package(minifi_options, package_manager)
    return True


def do_one_click_configuration(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    assert install_dependencies(minifi_options, package_manager)
    assert run_conan_install(minifi_options, package_manager)
    assert run_cmake(minifi_options, package_manager)
    return True


def main_menu(minifi_options: MinifiOptions, package_manager: PackageManager):
    done = False
    while not done:
        main_menu_options = {
            # All menu options' functions return True if the bootstrap should exit after execution
            f"Build dir: {minifi_options.build_dir}": build_dir_menu,
            f"Build type: {minifi_options.build_type.value}": build_type_menu,
            f"Custom malloc: {minifi_options.custom_malloc.value if minifi_options.custom_malloc is not None else 'N/A'}": custom_malloc_menu,
            "Build options": build_options_menu,
            "Extension options": extension_options_menu,
            "One click build": do_one_click_build,
            "Step by step build": step_by_step_menu,
            "Exit": lambda _options, _manager: True,
        }

        questions = [
            inquirer.List(
                "sub_menu",
                message="Main Menu",
                choices=[menu_option_name for menu_option_name in main_menu_options],
            ),
        ]

        main_menu_prompt = inquirer.prompt(questions)
        if main_menu_prompt is None:
            break
        done = main_menu_options[main_menu_prompt["sub_menu"]](minifi_options, package_manager)


def build_type_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    questions = [
        inquirer.List(
            "build_type",
            message="Build type",
            choices=minifi_options.build_type.possible_values,
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    minifi_options.build_type.value = answers["build_type"]
    minifi_options.save_option_state()
    return False


def custom_malloc_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    if minifi_options.custom_malloc is None:
        return False
    questions = [
        inquirer.List(
            "custom_malloc",
            message="Custom malloc implementation (only jemalloc and mimalloc are provided via Conan)",
            choices=minifi_options.custom_malloc.possible_values,
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    minifi_options.custom_malloc.value = answers["custom_malloc"]
    minifi_options.save_option_state()
    return False


def build_dir_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    questions = [
        inquirer.Path('build_dir',
                      message="Build directory",
                      default=minifi_options.build_dir
                      ),
    ]
    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    minifi_options.build_dir = answers["build_dir"]
    minifi_options.save_option_state()
    return False


def extension_options_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    possible_values = [option_name for option_name in minifi_options.extension_options]
    selected_values = [option.name for option in minifi_options.extension_options.values() if option.value == "ON"]
    questions = [
        inquirer.Checkbox(
            "options",
            message="MiNiFi C++ Extension Options (space to select, enter to confirm)",
            choices=possible_values,
            default=selected_values
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    for extension_option in minifi_options.extension_options.values():
        if extension_option.name in answers["options"]:
            extension_option.value = "ON"
        else:
            extension_option.value = "OFF"

    minifi_options.save_option_state()
    return False


def build_options_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    possible_values = [option_name for option_name in minifi_options.build_options]
    selected_values = [option.name for option in minifi_options.build_options.values() if option.value == "ON"]
    questions = [
        inquirer.Checkbox(
            "options",
            message="MiNiFi C++ Build Options (space to select, enter to confirm)",
            choices=possible_values,
            default=selected_values
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    for build_option in minifi_options.build_options.values():
        if build_option.name in answers["options"]:
            build_option.value = "ON"
        else:
            build_option.value = "OFF"

    minifi_options.save_option_state()
    return False


def step_by_step_menu(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    done = False
    while not done:
        step_by_step_options = {
            # All menu options' functions return True if the bootstrap should exit after execution
            f"Build dir: {minifi_options.build_dir}": build_dir_menu,
            "Install dependencies": install_dependencies,
            "Run conan install": run_conan_install,
            "Run cmake": run_cmake,
            "Build": do_build,
            "Package": do_package,
            "Docker build": do_docker_build,
            "Back": lambda _options, _manager: True,
        }
        questions = [
            inquirer.List(
                "selection",
                message="Step by step menu",
                choices=[step_by_step_menu_option_name for step_by_step_menu_option_name in step_by_step_options],
            ),
        ]

        step_by_step_prompt = inquirer.prompt(questions)
        if step_by_step_prompt is None:
            return True
        step_by_step_options[step_by_step_prompt["selection"]](minifi_options, package_manager)
        done = step_by_step_prompt['selection'] == 'Back'
    return False
