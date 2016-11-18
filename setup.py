"""A setuptools based setup module.

"""

# Always prefer setuptools over distutils
from setuptools import setup, find_packages
from setuptools.dist import Distribution
import distutils.log

# To use a consistent encoding
from codecs import open
import os
import subprocess

from setuptools.command.build_py import build_py
from distutils.command.build import build
from setuptools.command.install import install
from setuptools.command.develop import develop


class MyDist(Distribution):
     def has_ext_modules(self):
         return True

def sconsbuild(command_subclass):
    """A decorator for classes subclassing one of the setuptools commands.

    It modifies the run() method so that it also runs the scons build
    for the C shared library.
    """
    orig_run = command_subclass.run
    orig_initialize_options = command_subclass.initialize_options
    orig_finalize_options = command_subclass.finalize_options

    command_subclass.user_options.extend([
         ('scons-opts=', 's', 'SCons options'),
    ])

    def modified_initialize_options(self):
         self.scons_opts=''
         orig_initialize_options(self)

    def modified_finalize_options(self):
         orig_finalize_options(self)
         library_file_name.plat_name = self.plat_name

    def modified_run(self):
         scons_opts_list = [s.strip() for s in self.scons_opts.split(',')]
         command = ['scons']
         command.extend(scons_opts_list)
         command.append('pyinstall')

         self.announce('Building silentarmy C library (scons options: {})'.format(
              scons_opts_list), distutils.log.INFO)
         try:
              subprocess.check_call(command)
         except subprocess.CalledProcessError as e:
              self.announce('SCons build failed (command: {0}, {1})'.format(
                   ' '.join(command), command), distutils.log.ERROR)
              raise(e)
         else:
              orig_run(self)

    command_subclass.run = modified_run
    command_subclass.initialize_options = modified_initialize_options
    command_subclass.finalize_options = modified_finalize_options

    return command_subclass


@sconsbuild
class BuildCommand(build):
     pass


# Get the long description from the README file
with open(os.path.join(os.path.abspath(os.path.dirname(__file__)),
                       'README.rst'), encoding='utf-8') as f:
     long_description = f.read()

package = 'pysa'

setup(
     name=package,

     # Versions should comply with PEP440.  For a discussion on single-sourcing
     # the version across setup.py and the project code, see
     # https://packaging.python.org/en/latest/single_source_version.html
     version='1.0.0',

     description='Python Interface to libsilentarmy (GPU equihash solver for Zcash)',
     long_description=long_description,

     # The project's main homepage.
     url='https://github.com/honzik666/silentarmy',

     # Author details
     author='Jan Čapek',
     author_email='jan.capek@braiins.cz',

     # Choose your license
     license='MIT',

     # See https://pypi.python.org/pypi?%3Aaction=list_classifiers
     classifiers=[
          # How mature is this project? Common values are
          #   3 - Alpha
          #   4 - Beta
          #   5 - Production/Stable
          'Development Status :: 3 - Alpha',

          # Indicate who your project is intended for
          'Intended Audience :: Developers',

          # Pick your license as you wish (should match "license" above)
          'License :: OSI Approved :: MIT License',

          # Specify the Python versions you support here. In particular, ensure
          # that you indicate whether you support Python 2, Python 3 or both.
          'Programming Language :: Python :: 3.4',
          'Programming Language :: Python :: 3.5',
     ],

     # What does your project relate to?
     keywords='equihash solver GPU zcash mining',

     # You can just specify the packages manually here if your project is
     # simple. Or you can use find_packages().
     packages=find_packages(),

     eager_resources=['{0}/{1}'.format(package, library_file_name)],
     # List run-time dependencies here.  These will be installed by pip when
     # your project is installed. For an analysis of "install_requires" vs pip's
     # requirements files see:
     # https://packaging.python.org/en/latest/requirements.html
     # mako is suggested by pyopencl
     install_requires=['cffi'],

     package_data={
          package: ['*.so', '*.dll'],
     },
     # List additional groups of dependencies here (e.g. development
     # dependencies). You can install these using the following syntax,
     # for example:
     # $ pip install -e .[dev,test]
     extras_require={
     },
     cmdclass={
          'build': BuildCommand,
     },
     distclass = MyDist,
)
