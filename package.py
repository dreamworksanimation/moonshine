# -*- coding: utf-8 -*-
import os, sys

unittestflags = (['@run_all', '--unittest-xml', '--no-cache']
                 if os.environ.get('BROKEN_CUSTOM_ARGS_UNITTESTS') else [])

name = 'moonshine'

if 'early' not in locals() or not callable(early):
    def early(): return lambda x: x

@early()
def version():
    """
    Increment the build in the version.
    """
    _version = '10.12'
    from rezbuild import earlybind
    return earlybind.version(this, _version)

description = 'Shaders for Moonray'

authors = [
    'PSW Rendering and Shading',
    'moonbase-dev@dreamworks.com',
]

help = ('For assistance, '
        "please contact the folio's owner at: moonbase-dev@dreamworks.com")

if 'cmake' in sys.argv:
    build_system = 'cmake'
    build_system_pbr = 'cmake_modules'
else:
    build_system = 'scons'
    build_system_pbr = 'bart_scons-10'

variants = [
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2020.3', 'icc-19.0.5.281.x.2'],
    ['os-CentOS-7', 'opt_level-debug', 'refplat-vfx2020.3', 'icc-19.0.5.281.x.2'],
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2020.3', 'gcc-6.3.x.2'],
    ['os-CentOS-7', 'opt_level-debug', 'refplat-vfx2020.3', 'gcc-6.3.x.2'],
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2021.0', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-debug', 'refplat-vfx2021.0', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2021.0', 'clang-13', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2022.0', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-debug', 'refplat-vfx2022.0', 'gcc-9.3.x.1'],
]

conf_rats_variants = variants[0:2]

scons_targets = ['@install_include', '@install'] + unittestflags
proxy_targets = ['@rdl2proxy', '@install_SConscript']

sconsTargets = {
    'refplat-vfx2020.3': scons_targets,
    'refplat-vfx2021.0': scons_targets,
    'refplat-vfx2022.0': scons_targets,
}

requires = [
    'moonray-13.12',
    'openimageio-2',
    'scene_rdl2-11.11',
]

private_build_requires = [
    build_system_pbr,
    'cppunit',
    'ispc-1.14.1.x',
    'python-2.7|3.7|3.9'
]

commandstr = lambda i: "cd build/"+os.path.join(*variants[i])+"; ctest -j $(nproc)"
testentry = lambda i: ("variant%d" % i,
                       { "command": commandstr(i),
                         "requires": ["cmake-3.23"] + variants[i] } )
testlist = [testentry(i) for i in range(len(variants))]
tests = dict(testlist)

if build_system is 'cmake':
    def commands():
        prependenv('CMAKE_PREFIX_PATH', '{root}')
        prependenv('SOFTMAP_PATH', '{root}')
        prependenv('MOONRAY_DSO_PATH', '{root}/rdl2dso')
        prependenv('RDL2_DSO_PATH', '{root}/rdl2dso')
        prependenv('LD_LIBRARY_PATH', '{root}/lib')
        prependenv('PATH', '{root}/bin')
        prependenv('MOONRAY_CLASS_PATH', '{root}/coredata')
else:
    def commands():
        prependenv('SOFTMAP_PATH', '{root}')
        prependenv('MOONRAY_DSO_PATH', '{root}/rdl2dso')
        prependenv('RDL2_DSO_PATH', '{root}/rdl2dso')
        prependenv('LD_LIBRARY_PATH', '{root}/lib')
        prependenv('PATH', '{root}/bin')
        prependenv('MOONRAY_CLASS_PATH', '{root}/coredata')

uuid = 'f778de67-e145-411e-b958-b792e5ca016a'

config_version = 0
