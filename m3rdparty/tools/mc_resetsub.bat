set ROOT=%~dp0\..\
git submodule deinit --all -f
git submodule foreach git submodule deinit --all -f
git submodule foreach rm -rf .
