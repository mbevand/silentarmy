import os
import SCons.Scanner
import SCons.Action

test_env = Environment()
test_env.Append(CCFLAGS=['-std=gnu99', '-pedantic',  '-Wextra',
                         '-Wall', '-ggdb', '-Wno-deprecated-declarations',
                         '-Wno-overlength-strings'])
test_env.Append(LIBS=['OpenCL'])

lib_env = test_env.Clone()
lib_env.Append(CPPDEFINES=['SHARED_LIB'])
lib_env.Append(VARIANT_DIR='build-lib')
lib_env.Append(CCFLAGS=['-fPIC'])
test_env.Append(VARIANT_DIR='build-test')

common_src = ['main.c', 'blake.c', '_kernel.c', 'sha256.c']
lib_src = ['gpu_solver.c']

def get_source_names(env, sources):
    """Provides sources name relative to the variant directory (build
    directory)
    """
    return [os.path.join(env['VARIANT_DIR'], s) for s in sources]


def kernel(env):
    """
    Generates OpenCL kernel source
    """
    env.Command('$VARIANT_DIR/_kernel.c', 'input.cl',
                SCons.Action.Action(
                    """echo 'const char *ocl_code = R\"_mrb_(' > $TARGET;cat $SOURCE | $CC -E - >> $TARGET;echo ')_mrb_\";' >> $TARGET""",
                    'Generating OpenCL kernel: $TARGET, $SOURCE'),
                source_scanner=SCons.Scanner.C.CScanner())

test_env.VariantDir(test_env['VARIANT_DIR'], './', duplicate=0)
lib_env.VariantDir(lib_env['VARIANT_DIR'], './', duplicate=0)

kernel(test_env)
kernel(lib_env)

test_objs = test_env.Object(get_source_names(test_env, common_src))
lib_objs = lib_env.SharedObject(get_source_names(lib_env, common_src +
                                                 lib_src))

test_env.Program('$VARIANT_DIR/silentarmy', test_objs)
lib_env.SharedLibrary('$VARIANT_DIR/silentarmy', lib_objs)
