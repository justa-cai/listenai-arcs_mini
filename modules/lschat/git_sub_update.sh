west_lisa_commit=$(python3 ./.gitlab/query-yml.py west.yml manifest projects lisa revision)
git submodule update --init --recursive
cd lisa/
git fetch origin
git checkout ${west_lisa_commit}
git submodule update --init --recursive
cd ../
git_submodule_lisa_commit=$(git submodule status lisa | cut -c 2-41)
if [ "$west_lisa_commit" != "$git_submodule_lisa_commit" ]; then echo "commit do not match,$west_lisa_commit, $git_submodule_lisa_commit"; exit 1; fi