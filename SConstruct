"""
This SConstruct prepares 2 build environments:
- solver_env - used for building the sa_solver binary
- lib_env - used for building the shared library. This environment is a clone of solver_env with some modifications specific to building the library

Build targets are provided by SConscript.{solver,lib} resp.

Crossbuild with mingw to have a windows version is supported.

OpenCL headers detection is supported
"""
import os
import SCons.Scanner
import SCons.Action

default_cross_prefix = 'x86_64-w64-mingw32-'
AddOption('--cross-prefix',
          dest='cross_prefix',
          type='string',
          nargs=1,
          action='store',
          metavar='PREFIX',
          default='x86_64-w64-mingw32-',
          help='Cross toolchain prefix (default: {})'.format(default_cross_prefix))
AddOption('--enable-win-cross-build',
          dest='enable_win_cross_build',
          action='store_true',
          default=False,
          help='Enable cross build (default: {})'.format(False))
AddOption('--opencl-headers',
          dest='opencl_headers',
          type='string',
          action='store',
          default=None,
          help='OpenCL header path (default: {})'.format(None))
AddOption('--opencl-library',
          dest='opencl_library',
          type='string',
          action='store',
          default=None,
          help='OpenCL library path (default: {})'.format(None))


def kernel(env):
    """
    Generates OpenCL kernel source
    """
    k = env.Command('_kernel.c', 'input.cl',
                    SCons.Action.Action(
                        """echo 'const char *ocl_code = R\"_mrb_(' > $TARGET;cat $SOURCE | $CC -E - >> $TARGET;echo ')_mrb_\";' >> $TARGET""",
                        'Generating OpenCL kernel: $TARGET, $SOURCE'),
                    source_scanner=SCons.Scanner.C.CScanner())
    return k


solver_env = Environment(CROSS_PREFIX=GetOption('cross_prefix'))

solver_env.Append(CCFLAGS=['-std=gnu99', '-pedantic',  '-Wextra',
                         '-Wall', '-ggdb', '-Wno-deprecated-declarations',
                         '-Wno-overlength-strings'])

if GetOption('opencl_library') is not None:
    solver_env.Append(LIBPATH=[GetOption('opencl_library')])

if GetOption('opencl_headers') is not None:
    solver_env.Append(CPPPATH=[GetOption('opencl_headers')])

env_replace_options = {}
env_append_options = {}
if GetOption('enable_win_cross_build'):
    env_replace_options = {
        'AR': '${CROSS_PREFIX}ar',
        'AS': '${CROSS_PREFIX}as',
        'CC': '${CROSS_PREFIX}gcc',
        'CPP': '${CROSS_PREFIX}cpp',
        'CXX': '${CROSS_PREFIX}g++',
        'LD': '${CROSS_PREFIX}${SMARTLINK}',
        'RANLIB': '${CROSS_PREFIX}ranlib',
        'SHLIBSUFFIX': '.dll',
        'LIBPREFIX': '',
        'SHCCFLAGS': ['$CCFLAGS'],
    }
    env_append_options = {
        'CPPDEFINES': ['WINDOWS_PLATFORM', 
                       # this is needed to use mingw's printf not
                       # ms_printf (for format strings using int64_t)
                       '__USE_MINGW_ANSI_STDIO']
    }

solver_env.Replace(**env_replace_options)
solver_env.Append(**env_append_options)

if not solver_env.GetOption('clean'):
    conf = Configure(solver_env)
    if not conf.CheckLib('OpenCL'):
        print('Did not find OpenCL library, please specify with --opencl-library')
        Exit(1)
    if not conf.CheckCHeader('CL/cl.h'):
        print('Did not find OpenCL headers, please specify with --opencl-headers')
        Exit(1)


    solver_env = conf.Finish()


solver_env.Append(COMMON_SRC=['main.c', 'blake.c', 'sha256.c'])
solver_env.Append(KERNEL_GEN=kernel)

lib_env = solver_env.Clone()
lib_env.Append(CPPDEFINES=['SHARED_LIB'])

# make variant dirs part of the environments
lib_env.Append(VARIANT_DIR='build-lib')
solver_env.Append(VARIANT_DIR='build-solver')

# Don't compile the pure solver binary for windows as some parts have
# been disabled for mingw compilation
if not GetOption('enable_win_cross_build'):
    solver_env.SConscript('SConscript.solver', variant_dir=solver_env['VARIANT_DIR'],
                          exports='solver_env', duplicate=0)
lib_env.SConscript('SConscript.lib', variant_dir=lib_env['VARIANT_DIR'],
                   exports='lib_env', duplicate=0)
