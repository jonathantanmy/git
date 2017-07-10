This is still a work in progress. You can try it as follows:

Clone a repository and make it advertise the new feature.

    $ make prefix=$HOME/local install
    $ cd $HOME/tmp
    $ git clone https://github.com/git/git
    $ git -C git config advertiseblobmaxbytes 1

Generate bitmaps in the repo. This is needed because `rev-list`, the command
used to perform reachability checks when serving blobs, has [bugs] in the
absence of bitmaps.

[bugs]: https://public-inbox.org/git/20170309003547.6930-1-jonathantanmy@google.com/

    $ git -C git repack -a -d --write-bitmap-index

Perform the partial clone and check that it is indeed smaller. Specify
"file://" in order to test the partial clone mechanism. (If not, Git will
perform a local clone, which unselectively copies every object.)

Currently, partial clones **do not work with http/https/ftp** or any other
protocol requiring a remote helper that does not support `connect`.

    $ git clone --blob-max-bytes=0 "file://$(pwd)/git" git2
    $ du -sh git
    130M	git
    $ du -sh git2
    85M	git2

Observe that the new repo is automatically configured to fetch missing blobs
from the original repo.

    $ cat git2/.git/config
    [core]
    	repositoryformatversion = 0
    	filemode = true
    	bare = false
    	logallrefupdates = true
    	promisedblobcommand = git fetch-blob \"file:///[snip]/tmp/git\"
    [remote "origin"]
    	url = file:///[snip]/tmp/git
    	fetch = +refs/heads/*:refs/remotes/origin/*
    [branch "master"]
    	remote = origin
    	merge = refs/heads/master

When dealing with objects in the local repo, Git commands generally process
them one at a time; invoking "promisedblobcommand" is no exception. But in this
branch, "git checkout" has been optimized to fetch missing blobs all at once.
This can be observed as follows:

    $ cd git2
    $ git config core.promisedblobcommand "tee blobs | $(git config core.promisedblobcommand)"
    $ git checkout HEAD^
    Counting objects: 2975, done.
    Compressing objects: 100% (2688/2688), done.
    Total 2975 (delta 243), reused 1640 (delta 220)
    pack	7d821a2a4cf0c1936312a478b2b84a42796317b0
    Note: checking out 'HEAD^'.
    
    You are in 'detached HEAD' state. You can look around, make experimental
    changes and commit them, and you can discard any commits you make in this
    state without impacting any branches by performing another checkout.
    
    If you want to create a new branch to retain commits you create, you may
    do so (now or later) by using -b with the checkout command again. Example:
    
      git checkout -b <new-branch-name>
    
    HEAD is now at 5e5a7cd93... Sixteenth batch for 2.14
    $ wc -l blobs
    3064 blobs
    
