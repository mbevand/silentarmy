Local Development Build (Linux)
===============================

* optionally, enable python virtualenv::
   . .zcashvenv3/bin/activate
* development installation in case of OpenCL present in system pathf::
   pip install -e .
* development installation with local OpenCL headers and library
  (adjust library/header path and name depending on the version
  used))::
   pip install --global-option=build  --global-option="--scons-opts=--opencl-headers=/path/to/AMD APP SDK/3.0/include/,--opencl-library=/path/to/AMD APP SDK/3.0/bin/x86_64"  -e .

* Rebuilding only the shared library (opencl header and library can be
  omitted when both are system wide present)::
   scons --opencl-headers=/path/to/AMD APP SDK/3.0/include/ --opencl-library=/path/to/AMD APP SDK/3.0/bin/x86_64 pyinstall



Building Wheel for Windows 64-bit
=================================

* make sure you have OpenCL library and headers installed
* optionally source a virtualenv
* run setup to build the wheel - note the platform name specification
  for the 64-bit system and path to x86_64 variant of the OpenCL
  libraries::
   python setup.py build --scons-opts='--enable-win-cross-build,--opencl-headers=/path/to/AMD APP SDK/3.0/include/,--opencl-library=/AMD APP SDK/3.0/bin/x86_64' bdist_wheel --plat-name='win_amd64'
