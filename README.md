This branch contains the hg-git's map file between git and hg objects.

To find the hg revision from a git name, add show-hg-rev alias to your clone's `.git/config` :

`
[alias]
show-hg-rev=!sh -c 'git show hg-git:git-mapfile | grep `git log -1 --pretty=format:%H "$0"` | cut -d \" \" -f 2'
`

Example usage to get the devtools tip revision hash:
`
$ git show-hg-rev devtools
fae9a993f933f4507d5f432dea70f790c6d46af2
`

