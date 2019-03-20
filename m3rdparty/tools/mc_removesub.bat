git submodule deinit -f %1
git rm --cached %1
rm -rf .git/modules/%1
rm -rf %1
git reset HEAD .gitmodules
git config -f .gitmodules --remove-section submodule.%1
git add --all
git commit -m "Removed"