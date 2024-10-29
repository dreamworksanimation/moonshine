# Copyright 2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

# -*- coding: utf-8 -*-
import os

name = 'moonshine'

if 'early' not in locals() or not callable(early):
    def early(): return lambda x: x

@early()
def version():
    """
    Increment the build in the version.
    """
    _version = '13.30'
    from rezbuild import earlybind
    return earlybind.version(this, _version)

description = 'Shaders for Moonray'

authors = [
    'PSW Rendering and Shading',
    'moonbase-dev@dreamworks.com',
]

help = ('For assistance, '
        "please contact the folio's owner at: moonbase-dev@dreamworks.com")

variants = [
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2021.0', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-debug', 'refplat-vfx2021.0', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2021.0', 'clang-13'],
    ['os-CentOS-7', 'opt_level-optdebug', 'refplat-vfx2022.0', 'gcc-9.3.x.1'],
    ['os-CentOS-7', 'opt_level-debug', 'refplat-vfx2022.0', 'gcc-9.3.x.1'],
    ['os-rocky-9', 'opt_level-optdebug', 'refplat-vfx2021.0', 'gcc-9.3.x.1'],
    ['os-rocky-9', 'opt_level-debug', 'refplat-vfx2021.0', 'gcc-9.3.x.1'],
    ['os-rocky-9', 'opt_level-optdebug', 'refplat-vfx2022.0', 'gcc-9.3.x.1'],
    ['os-rocky-9', 'opt_level-debug', 'refplat-vfx2022.0', 'gcc-9.3.x.1'],
    ['os-rocky-9', 'opt_level-optdebug', 'refplat-vfx2023.1', 'gcc-11.x'],
    ['os-rocky-9', 'opt_level-debug', 'refplat-vfx2023.1', 'gcc-11.x'],
    ['os-rocky-9', 'opt_level-optdebug', 'refplat-vfx2023.1', 'clang-17.0.6.x'],
#    ['os-rocky-9', 'opt_level-debug', 'refplat-vfx2023.1', 'clang-17.0.6.x'],
]

conf_rats_variants = variants[0:2]
conf_CI_variants = list(filter(lambda v: 'os-CentOS-7' in v, variants))

requires = [
    'moonray-16.30',
    'openimageio-2.3.20.0.x',
    'scene_rdl2-14.17',
]

private_build_requires = [
    'cmake_modules-1.0',
    'cppunit',
    'ispc-1.20.0.x',
    'python-2.7|3.7|3.9|3.10'
]

commandstr = lambda i: "cd build/"+os.path.join(*variants[i])+"; ctest -j $(nproc)"
testentry = lambda i: ("variant%d" % i,
                       { "command": commandstr(i),
                         "requires": ["cmake-3.23"] + variants[i] } )
testlist = [testentry(i) for i in range(len(variants))]
tests = dict(testlist)

def commands():
    prependenv('CMAKE_MODULE_PATH', '{root}/lib64/cmake')
    prependenv('CMAKE_PREFIX_PATH', '{root}')
    prependenv('SOFTMAP_PATH', '{root}')
    prependenv('MOONRAY_DSO_PATH', '{root}/rdl2dso')
    prependenv('RDL2_DSO_PATH', '{root}/rdl2dso')
    prependenv('LD_LIBRARY_PATH', '{root}/lib64')
    prependenv('PATH', '{root}/bin')
    prependenv('MOONRAY_CLASS_PATH', '{root}/coredata')

uuid = 'f778de67-e145-411e-b958-b792e5ca016a'

config_version = 0
