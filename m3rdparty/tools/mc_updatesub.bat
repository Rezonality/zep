set ROOT=%~dp0
git pull
git submodule init
git submodule sync
git submodule update
git submodule foreach git submodule sync
git submodule foreach git submodule update
git submodule foreach git checkout master
git submodule foreach git pull origin master