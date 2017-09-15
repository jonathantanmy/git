This is still a work in progress, but you can try it (see Demo).

Demo
====

Obtain a repository.

    $ make prefix=$HOME/local install
    $ cd $HOME/tmp
    $ git clone https://github.com/git/git

Make it advertise the new feature and allow requests for arbitrary blobs.

    $ git -C git config uploadpack.advertiseblobmaxbytes 1
    $ git -C git config uploadpack.allowanysha1inwant 1

Perform the partial clone and check that it is indeed smaller. Specify
"file://" in order to test the partial clone mechanism. (If not, Git will
perform a local clone, which unselectively copies every object.)

    $ git clone --blob-max-bytes=100000 "file://$(pwd)/git" git2
    $ git clone "file://$(pwd)/git" git3
    $ du -sh git2 git3
    116M	git2
    129M	git3

Observe that the new repo is automatically configured to fetch missing objects
from the original repo. Subsequent fetches will also be partial.

    $ cat git2/.git/config
    [core]
    	repositoryformatversion = 1
    	filemode = true
    	bare = false
    	logallrefupdates = true
    [remote "origin"]
    	url = [snip]
    	fetch = +refs/heads/*:refs/remotes/origin/*
    	blobmaxbytes = 100000
    [extensions]
    	partialclone = origin
    [branch "master"]
    	remote = origin
    	merge = refs/heads/master

Unlike in an older version of this code (see the `partialclone` branch), this
also works with the HTTP/HTTPS protocols.

Design
======

Local repository layout
-----------------------

A repository declares its dependence on a *promisor remote* (a remote that
declares that it can serve certain objects when requested) by a repository
extension "partialclone". `extensions.partialclone` must be set to the name of
the remote ("origin" in the demo above).

A packfile can be annotated as originating from the promisor remote by the
existence of a "(packfile name).promisor" file with arbitrary contents (similar
to the ".keep" file). Whenever a promisor remote sends an object, it declares
that it can serve every object directly or indirectly referenced by the sent
object.

A promisor packfile is a packfile annotated with the ".promisor" file. A
promisor object is an object in a promisor packfile. A promised object is an
object directly referenced by a promisor object.

(In the future, we might need to add ".promisor" support to loose objects.)

Connectivity check and gc
-------------------------

The object walk done by the connectivity check (as used by fsck and fetch) stops
at all promisor objects and promised objects.

The object walk done by gc also stops at all promisor objects and promised
objects. Only non-promisor packfiles are deleted (if pack deletion is
requested); promisor packfiles are left alone. This maintains the distinction
between promisor packfiles and non-promisor packfiles. (In the future, we might
need to do something more sophisticated with promisor packfiles.)

Fetching of promised objects
----------------------------

When `sha1_object_info_extended()` (or similar) is invoked, it will
automatically attempt to fetch a missing object from the promisor remote if that
object is not in the local repository. For efficiency, no check is made as to
whether that object is a promised object or not.

This automatic fetching can be toggled on and off by the `fetch_if_missing`
global variable, and it is on by default.

The actual fetch is done through the fetch-pack/upload-pack protocol. Right now,
this uses the fact that upload-pack allows blob and tree "want"s, and this
incurs the overhead of the unnecessary ref advertisement. I hope that protocol
v2 will allow us to declare that blob and tree "want"s are allowed, and allow
the client to declare that it does not want the ref advertisement. All packfiles
downloaded in this way are annotated with ".promisor".

Fetching with `git fetch`
-------------------------

The fetch-pack/upload-pack protocol has also been extended to support omission
of blobs above a certain size. The client only allows this when fetching from
the promisor remote, and will annotate any packs received from the promisor
remote with ".promisor".
