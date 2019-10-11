pushd build
cmake --build . --config Release
mkdir c:\tools\Zep
xcopy /s Release\* c:\tools\Zep
popd


