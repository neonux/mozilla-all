#!/usr/bin/env bash
CENTRAL_REPO="http://hg.mozilla.org/mozilla-central"
INBOUND_REPO="http://hg.mozilla.org/integration/mozilla-inbound"
FXTEAM_REPO="http://hg.mozilla.org/integration/fx-team"
DEVTOOLS_REPO="http://hg.mozilla.org/projects/devtools"
GFX_REPO="http://hg.mozilla.org/projects/graphics"
E10S_REPO="http://hg.mozilla.org/projects/electrolysis"
JM_REPO="http://hg.mozilla.org/projects/jaegermonkey"
UX_REPO="http://hg.mozilla.org/projects/ux"

REPOS="$CENTRAL_REPO $INBOUND_REPO $FXTEAM_REPO $DEVTOOLS_REPO $GFX_REPO $E10S_REPO $JM_REPO $UX_REPO"
for REPO in $REPOS
do
  NAME=`basename $REPO`
  TIP_REV=`hg id $REPO`
  hg pull $REPO
  GIT_REV=`grep $TIP_REV .hg/git-mapfile | cut -d " " -f 1`
  if [ $? -eq 0 ]; then
    if [ $GIT_REV ]; then
      echo $GIT_REV > .hg/git/refs/heads/$NAME
      echo "* updated git ref $NAME to $GIT_REV"
    fi
  fi
done

echo "exporting to git..."
hg gexport
echo "exported!"

echo "updating map file..."
GIT_DIR=.hg/git
export GIT_DIR
GIT_WORK_TREE=hg-git
export GIT_WORK_TREE

ln -f .hg/git-mapfile hg-git/git-mapfile
git checkout -f git-mapfile
git add git-mapfile
git commit -m "Update map file"
echo "updated map file!"

git push --all git-bare-mirror
