call config_all.bat
cd build
cmake --build . --config Release --target Install
cmake --build . --config Debug --target Install
cd ..
