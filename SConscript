Import('env')

import os
import SCons.Script

env.Tool('component')
env.Tool('dwa_install')
env.Tool('dwa_run_test')
env.Tool('dwa_utils')
env.Tool('python_sdk')

from dwa_sdk import DWALoadSDKs
DWALoadSDKs(env)

env.AppendUnique(ISPCFLAGS = ['--dwarf-version=2', '--wno-perf'])

# Suppress depication warning from tbb-2020.
env.AppendUnique(CPPDEFINES='TBB_SUPPRESS_DEPRECATED_MESSAGES')

# SDKScript generation template and dictionary primed from location variables.
# This defines all that scons builds need.
SDKScriptTMPL = """##-*-python-*-

Import('env')

env.GatherProxies()
"""

# Generate the SDKScript file from the template and dictionary.
SDKScript = env.Textfile(target='SDKScript', source=SDKScriptTMPL,
                         TEXTFILESUFFIX='')

# Install the generated SDKScript,
SDKScript = env.DWAInstallSDKScript(SDKScript)
env.Alias('@rdl2proxy', SDKScript)
env.NoCache(SDKScript)

# For Arras, we've made the decision to part with the studio standards and use #pragma once
# instead of include guards. We disable warning 1782 to avoid the related (incorrect) compiler spew.
if 'icc' in env['CC'] and '-wd1782' not in env['CXXFLAGS']:
    env['CXXFLAGS'].append('-wd1782') 	   # #pragma once is obsolete. Use #ifndef guard instead.
    env['CXXFLAGS'].append('-wd1684')      # conversion from pointer to same-sized integral type
elif 'gcc' in env['CC']:
    env['CXXFLAGS'].append('-D__AVX__')    # Force OpenImageIO to use AVX over SSE
    env['CXXFLAGS'].append('-w')           #TODO: I just want to keep the output clear until we have a running build
    env['CXXFLAGS'].append('-fpermissive') #TODO: There's some typedef collisions with moonray that pop up.  
elif 'clang' in env['CC']:
    env['CXXFLAGS'].append('-Wno-register') # E: Ignore lib/mm's use of register keyword.
    env['CXXFLAGS'].append('-Wno-unknown-warning-option') # E: work around geometry's lack of clang support.

# Create include/moonray link to ../lib in the build directory.
Execute("mkdir -p include")
Execute("rm -f include/moonray")
Execute("ln -sfv ../lib include/moonshine")
env.Append(CPPPATH=[env.Dir('#include')])
env.Append(CPPPATH=[env.Dir('$INSTALL_DIR/include')])
env.Append(CPPPATH=[env.Dir('include')])
env.Append(ISPC_CPPPATH=[env.Dir('include')])

env.DWASConscriptWalk(topdir='#lib', ignore=[])
env.DWASConscriptWalk(topdir='#dso', ignore=[])
#env.DWASConscriptWalk(topdir='#cmd', ignore=[])
env.DWAResolveUndefinedComponents(['#SConscripts'])
env.DWAFillInMissingInitPy()
env.DWAFreezeComponents()

# Set default target
env.Default([env.Alias('@install_include'), env.Alias('@install')])
